#include <Arduino.h>
#include <Update.h>
#include <ESPAsyncWebServer.h>
#include "mbedtls/sha256.h"
#include "knomi.h"

//
// kn0m3 OTA — a small, reliable replacement for BTT's AsyncElegantOTA.
//
// Why this exists: the stock firmware bundled a fork of AsyncElegantOTA that
// *required* an MD5 form field and returned HTTP 400 ("Bad Request") on every
// failure, hiding the real cause. Worse, MD5 is the wrong tool: the ESP32 app
// image already carries an appended SHA-256 that the bootloader verifies when a
// slot is activated (Update.end() -> esp_ota_set_boot_partition), so a corrupt
// upload can never boot. This handler leans on that hardware-backed SHA-256 for
// integrity, optionally adds an end-to-end SHA-256 transport check, requires no
// MD5, and reports genuine failures as a readable HTTP 500.
//

static mbedtls_sha256_context ota_sha;
static String ota_expected_sha;   // optional client-supplied SHA-256 (lowercase hex)
static bool   ota_check_sha;
static String ota_error;          // first failure message, "" == ok so far

static void ota_fail(const String &msg) {
    if (ota_error.isEmpty()) {
        ota_error = msg;
        Serial.print("[ota] FAIL: ");
        Serial.println(msg);
    }
}

static void ota_on_upload(AsyncWebServerRequest *request, String filename,
                          size_t index, uint8_t *data, size_t len, bool final) {
    if (index == 0) {
        ota_error = "";
        // Optional transport check: a "sha256" field (sent before the firmware,
        // e.g. by tools/ota_flash.sh). If absent we rely on image validation.
        ota_expected_sha = "";
        if (request->hasParam("sha256", true))
            ota_expected_sha = request->getParam("sha256", true)->value();
        ota_expected_sha.toLowerCase();
        ota_check_sha = (ota_expected_sha.length() == 64);

        mbedtls_sha256_init(&ota_sha);
        mbedtls_sha256_starts_ret(&ota_sha, 0);   // 0 = SHA-256 (not SHA-224)

        int cmd = (filename == "filesystem") ? U_SPIFFS : U_FLASH;
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
            ota_fail(String("OTA begin failed: ") + Update.errorString());
            Update.printError(Serial);
        }
        Serial.printf("[ota] start '%s' (%s)\r\n", filename.c_str(),
                      ota_check_sha ? "sha256 supplied" : "image-validated");
    }

    if (ota_error.isEmpty() && len) {
        mbedtls_sha256_update_ret(&ota_sha, data, len);
        if (Update.write(data, len) != len) {
            ota_fail(String("OTA write failed: ") + Update.errorString());
            Update.printError(Serial);
        }
    }

    if (final) {
        uint8_t digest[32];
        char hex[65];
        mbedtls_sha256_finish_ret(&ota_sha, digest);
        mbedtls_sha256_free(&ota_sha);
        for (int i = 0; i < 32; i++) sprintf(hex + i * 2, "%02x", digest[i]);

        if (ota_error.isEmpty() && ota_check_sha && ota_expected_sha != hex)
            ota_fail("SHA-256 mismatch — upload corrupted in transit");

        // Update.end() validates the image's own appended SHA-256 before
        // activating the slot, so this is the authoritative integrity gate.
        if (ota_error.isEmpty() && !Update.end(true)) {
            ota_fail(String("OTA verify failed: ") + Update.errorString());
            Update.printError(Serial);
        }
        if (ota_error.isEmpty())
            Serial.printf("[ota] success — sha256=%s\r\n", hex);
    }
}

static const char OTA_PAGE[] PROGMEM = R"HTML(<!DOCTYPE html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>kn0m3 OTA</title><style>
body{font-family:Arial,Helvetica,sans-serif;max-width:520px;margin:40px auto;padding:0 16px}
h1{color:#C02E2F}.box{border:1px solid #ccc;border-radius:8px;padding:20px}
progress{width:100%;height:18px}#msg{margin-top:12px;white-space:pre-wrap;font-family:monospace}
button{background:#C02E2F;color:#fff;border:0;padding:10px 18px;border-radius:5px;font-size:16px;cursor:pointer}
</style></head><body>
<h1>kn0m3 firmware update</h1>
<div class="box">
<p>Select a <code>firmware.bin</code> for this board and flash it. The device verifies the
image's SHA-256 and reboots automatically. No MD5, no surprises.</p>
<input type="file" id="f" accept=".bin"><br><br>
<button onclick="up()">Upload &amp; flash</button>
<p><progress id="p" value="0" max="100" hidden></progress></p>
<div id="msg"></div>
</div>
<script>
function msg(t){document.getElementById('msg').textContent=t}
function up(){
  var f=document.getElementById('f').files[0];
  if(!f){msg('Choose a .bin file first.');return}
  var fd=new FormData();fd.append('firmware',f,f.name);
  var x=new XMLHttpRequest(),p=document.getElementById('p');p.hidden=false;
  x.upload.onprogress=function(e){if(e.lengthComputable)p.value=Math.round(e.loaded/e.total*100)};
  x.onload=function(){msg(x.status==200?'OK — rebooting into the new firmware...':'FAILED ('+x.status+'): '+x.responseText)};
  x.onerror=function(){msg('Connection closed (the device may be rebooting).')};
  x.open('POST','/update');x.send(fd);
}
</script></body></html>)HTML";

void ota_setup(AsyncWebServer *server) {
    server->on("/update/identity", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(200, "application/json",
                  String("{\"id\": \"") + FW_VERSION + "\", \"hardware\": \"ESP32\"}");
    });

    server->on("/update", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send_P(200, "text/html", OTA_PAGE);
    });

    server->on("/update", HTTP_POST,
        [](AsyncWebServerRequest *req) {
            bool ok = ota_error.isEmpty();
            AsyncWebServerResponse *res =
                req->beginResponse(ok ? 200 : 500, "text/plain", ok ? "OK" : ota_error);
            res->addHeader("Connection", "close");
            req->send(res);
            if (ok) { delay(200); ESP.restart(); }
        },
        ota_on_upload);
}
