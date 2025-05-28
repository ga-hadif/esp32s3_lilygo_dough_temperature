const char* indexHtml = R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
    <meta charset="UTF-8" />
    <title>ESP32 Wi-Fi Config</title>
    <style>
      body {
        font-family: Arial, Helvetica, sans-serif;
        background-color: #181818;
        color: #efefef;
      }
      input,
      select {
        width: 100%;
        padding: 12px 20px;
        margin: 8px 0;
        display: inline-block;
        border: 1px solid #ccc;
        border-radius: 4px;
        box-sizing: border-box;
      }
      button {
        background-color: #4caf50;
        color: white;
        padding: 14px 20px;
        margin: 8px 0;
        font-size: 16px;
        border: none;
        cursor: pointer;
        width: 100%;
        border-radius: 4px;
      }
      button:hover {
        opacity: 0.8;
      }
      .scan-wifi {
        width: 200px;
        background-color: #007bff;
        color: white;
        padding: 0.5rem 1rem;
        font-size: 16px;
        border: none;
        border-radius: 6px;
        font-weight: bold;
        cursor: pointer;
        transition: 0.2s ease;
      }
      .scan-wifi:hover {
        background-color: #0056b3;
      }
      #wifiModal {
        display: none;
        position: fixed;
        top: 0;
        left: 0;
        width: 100%;
        height: 100%;
        background-color: rgba(0, 0, 0, 0.7);
        z-index: 1000;
        align-items: center;
        justify-content: center;
      }
      #wifiModal.show {
        display: flex;
      }
      .modal-content {
        background-color: #fff;
        color: black;
        padding: 1rem;
        border-radius: 10px;
        width: 300px;
        max-height: 400px;
        overflow-y: auto;
      }
      .modal-content li {
        padding: 8px;
        border-bottom: 1px solid #ddd;
        cursor: pointer;
      }
      .modal-content li:hover {
        background-color: #eee;
      }
    </style>
  </head>
  <body>
    <div style="display: flex; align-items: center; justify-content: left; gap: 1rem;">
      <h2>Select Wi-Fi Networks</h2>
      <button id="scanWifiButton" class="scan-wifi">Scan Wi-Fi</button>
    </div>
    <form action="/save" method="POST">
      <label for="ssid">SSID:</label><br />
      <input type="text" id="ssid" name="ssid" /><br /><br />

      <label for="password">Password:</label><br />
      <input type="password" id="password" name="password" /><br /><br />

      <button type="submit">Save Wi-Fi Credentials</button>
    </form>

    <form>
      <label for="name">Device Name:</label><br />
      <input type="text" id="name" name="name" /><br /><br />

    </form>

    <div id="wifiModal">
      <div class="modal-content">
        <h3 style="text-align: center;">Wi-Fi</h3>
        <ul id="ssidList" style="list-style: none; padding: 0;"></ul>
        <button onclick="closeModal()" style="margin-top: 1rem;">Close</button>
      </div>
    </div>

    <script>
  const modal = document.getElementById("wifiModal");
  const ssidList = document.getElementById("ssidList");
  const ssidInput = document.getElementById("ssid");

  function closeModal() {
    modal.classList.remove("show");
  }

  document.getElementById("scanWifiButton").addEventListener("click", startScan);

  function startScan() {
    ssidList.innerHTML = "<li>Scanning...</li>";
    modal.classList.add("show");

    // Start scan, then start polling for results
    fetch("/scan");
    checkScanResult();
  }

  function checkScanResult() {
    fetch("/scanResult")
      .then((res) => res.json())
      .then((data) => {
        if (data.status === "scanning") {
          ssidList.innerHTML = "<li>Scanning... please wait</li>";
          setTimeout(checkScanResult, 2000); // try again in 2 seconds
          return;
        }

        if (data.status === "done") {
          ssidList.innerHTML = "";
          data.ssids.forEach((ssid) => {
            const item = document.createElement("li");
            item.textContent = ssid;
            item.addEventListener("click", () => {
              ssidInput.value = ssid;
              closeModal();
            });
            ssidList.appendChild(item);
          });
        }
      })
      .catch(() => {
        ssidList.innerHTML = "<li>Error fetching results</li>";
      });
  }

  window.addEventListener("load", () => {
  fetch("/wifi_config")
    .then(res => res.json())
    .then(data => {
      document.getElementById("ssid").value = data.ssid || "";
      document.getElementById("password").value = data.pass || "";
    });
});


  </body>
</html>
)rawliteral";
