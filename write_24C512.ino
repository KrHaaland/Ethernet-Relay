#include <Wire.h>

#define EEPROM_I2C_ADDRESS 0x50  // The I2C address for the 24C512 EEPROM

// Your PROGMEM declaration here
const char webpage[] PROGMEM = R"=====(
<html>
<head><title>Relay Module</title></head>
<body>
<h2 style="margin-bottom: 0px;">Relay1</h2>
<input type='button' id='offRelay1' value='OFF'>
<input type='button' id='onRelay1' value='ON'>
<br>
<b>Current State:  </b> <span id='relay1status'></span><br>
<h2 style="margin-bottom: 0px;">Relay2</h2>
<input type='button' id='offRelay2' value='OFF'>
<input type='button' id='onRelay2' value='ON'>
<br>
<b>Current State:  </b> <span id='relay2status'></span><br>
<h2 style="margin-bottom: 0px;">MQTT Status</h2>
<b>Connected:  </b><span id='mqtt_connected'></span><br>
<b>State:  </b><span id='mqtt_status'></span><br>
<h2 style="margin-bottom: 0px;">Current MQTT Configuration</h2>
<b>Server:  </b><span id='mqtt_current_ip'></span><br>
<b>Topic:  </b><span id='mqtt_current_topic'></span><br>
<b>Update interval:  </b><span id='mqtt_current_update_interval'></span><br>
<h2 style="margin-bottom: 0px;">Current Network Configuration</h2>
<b>Network mode:  </b><span id='device_current_networkmode'></span><br>
<b>Device IP:  </b><span id='device_current_ip'></span><br>
<b>Device Subnet:  </b><span id='device_current_subnet'></span><br>
<b>Device Gateway:  </b><span id='device_current_gw'></span><br>
<b>Device DNS:  </b><span id='device_current_dns'></span><br>
<br><br>
<h2 style="margin-bottom: 0px;">Device Settings</h2>
<form id="inputForm" action="/" method="GET">
MQTT-Server: <br> <input type="text" maxlength="63" name="in1"><br>
MQTT-Topic: <br> <input type="text" maxlength="63" name="in2"><br>
Update interval (in seconds): <br><input type="text" name="in3"><br><br>
Network mode:<br>
<select name="net" id="DHCP">
  <option value="2">Unchanged</option>
  <option value="0">Static</option>
  <option value="1">DHCP</option>
</select><br>
Device IP: <br> <input type="text" id="in4" name="in4" size="15" maxlength="15"><br>
Device Subnet: <br> <input type="text" id="in5" name="in5" size="15" maxlength="15"><br>
Device Gateway: <br> <input type="text" id="in6" name="in6" size="15" maxlength="15"><br>
Device DNS: <br> <input type="text" id="in7" name="in7" size="15" maxlength="15"><br>
<input type='submit' value='Submit'>
<br><br>
<input type='button' id='reboot' value='Reboot'>
</form>
</body>
</html>

<script>
function updateValues() {
    fetch('/getData')
        .then(response => response.json()) // Parse JSON response
        .then(data => {
            // Update each element with the corresponding data
            document.getElementById('relay1status').innerText = data.relay1status;
            document.getElementById('relay2status').innerText = data.relay2status;
            document.getElementById('mqtt_connected').innerText = data.mqtt_connected;
            document.getElementById('mqtt_status').innerText = data.mqtt_status;
            document.getElementById('mqtt_current_ip').innerText = data.mqtt_current_ip;
            document.getElementById('mqtt_current_topic').innerText = data.mqtt_current_topic;
            document.getElementById('mqtt_current_update_interval').innerText = data.mqtt_current_update_interval;
            document.getElementById('device_current_networkmode').innerText = data.device_current_networkmode;    
            document.getElementById('device_current_ip').innerText = data.device_current_ip;            
            document.getElementById('device_current_subnet').innerText = data.device_current_subnet;
            document.getElementById('device_current_gw').innerText = data.device_current_gw;
            document.getElementById('device_current_dns').innerText = data.device_current_dns;
        })
        .catch(error => console.error('Error:', error));
}

updateValues();

setInterval(updateValues, 2000);

function validateInputs() {
    var input1Value = document.querySelector("input[name='input1']").value;
    var input2Value = document.querySelector("input[name='input2']").value;

    var validPattern = /^[A-Za-z0-9\/.]*$/;  // Allows empty string as well

    if (input1Value !== '' && !validPattern.test(input1Value)) {
        alert("Input1 can only contain digits, letters, '/' and '.'.");
        return false;
    }

    if (input2Value !== '' && !validPattern.test(input2Value)) {
        alert("Input2 can only contain digits, letters, '/' and '.'.");
        return false;
    }
    return true;
}

document.querySelector("form").addEventListener("submit", function(event) {
    if (!validateInputs()) {
        event.preventDefault();
    }
});

function validateInput3() {
    var input3Value = document.querySelector("input[name='input3']").value;

    // Check if the input is empty
    if (input3Value === '') {
        return true;
    }

    if (!/^\d+$/.test(input3Value) || input3Value < 1 || input3Value > 65000) {
        alert("Input must be a number between 1 and 65000.");
        return false;
    }
    return true;
}

document.querySelector("form").addEventListener("submit", function(event) {
    if (!validateInput3()) {
        event.preventDefault();
    }
});

function validateIpAddress(ipVal) {
    if (ipVal === "") {
        return true;
    }
    let parts = ipVal.split('.');
    if (parts.length !== 4) {
        return false;
    }
    for (let i = 0; i < parts.length; i++) {
        let part = parseInt(parts[i], 10);
        if (isNaN(part) || part < 0 || part > 255) {
            return false;
        }
    }
    return true;
}

document.getElementById('inputForm').onsubmit = function(event) {
    let DeviceIpAddress = document.getElementById('in4').value;
    let DeviceSubnetAddress = document.getElementById('in5').value;
    let DeviceGatewayAddress = document.getElementById('in6').value;
    let DeviceDNSAddress = document.getElementById('in7').value;

    if (!validateIpAddress(DeviceIpAddress)) {
        alert('IP Address must be a valid IP (each part must be a number between 0 and 255)');
        event.preventDefault();
        return false;
    }
    if (!validateIpAddress(DeviceSubnetAddress)) {
        alert('Subnet Mask must be a valid IP (each part must be a number between 0 and 255)');
        event.preventDefault();
        return false;
    }
    if (!validateIpAddress(DeviceGatewayAddress)) {
        alert('Gateway Address must be a valid IP (each part must be a number between 0 and 255)');
        event.preventDefault();
        return false;
    }
    if (!validateIpAddress(DeviceDNSAddress)) {
        alert('DNS Server must be a valid IP (each part must be a number between 0 and 255)');
        event.preventDefault();
        return false;
    }
};

document.getElementById('offRelay1').addEventListener('click', function() {
    fetch('/?offRelay1', { method: 'GET' })
        .then(response => response.text())
        .then(data => console.log(data))
        .catch(error => console.error('Error:', error));
});

document.getElementById('onRelay1').addEventListener('click', function() {
    fetch('/?onRelay1', { method: 'GET' })
        .then(response => response.text())
        .then(data => console.log(data))
        .catch(error => console.error('Error:', error));
});

document.getElementById('offRelay2').addEventListener('click', function() {
    fetch('/?offRelay2', { method: 'GET' })
        .then(response => response.text())
        .then(data => console.log(data))
        .catch(error => console.error('Error:', error));
});

document.getElementById('onRelay2').addEventListener('click', function() {
    fetch('/?onRelay2', { method: 'GET' })
        .then(response => response.text())
        .then(data => console.log(data))
        .catch(error => console.error('Error:', error));
});

document.getElementById('reboot').addEventListener('click', function() {
    fetch('/?reboot', { method: 'GET' })
        .then(response => response.text())
        .then(data => console.log(data))
        .catch(error => console.error('Error:', error));
});
</script>
)=====";

void setup() {
  Serial.begin(115200);
  Wire.begin();

  // Write data to EEPROM
  writeEEPROMData();

  // Read data from EEPROM
  readEEPROMData();

  while(1);
}

void loop() {
}

void writeEEPROMData() {
  for (int i = 0; i < strlen_P(webpage); i++) {
    char c = pgm_read_byte_near(webpage + i);
    writeEEPROM(i, c);
    delay(5);  // Small delay to ensure EEPROM write cycle completes
  }
  Serial.println("Write complete");
}

void writeEEPROM(int address, char data) {
  Wire.beginTransmission(EEPROM_I2C_ADDRESS);
  Wire.write((address >> 8) & 0xFF);  // MSB
  Wire.write(address & 0xFF);         // LSB
  Wire.write(data);
  Wire.endTransmission();
}

void readEEPROMData() {
  for (int i = 0; i < 7864; i++) {
    char data = readEEPROM(i);
    Serial.print(data);
  }
}

char readEEPROM(int address) {
  Wire.beginTransmission(EEPROM_I2C_ADDRESS);
  Wire.write((address >> 8) & 0xFF);  // MSB
  Wire.write(address & 0xFF);         // LSB
  Wire.endTransmission();
  
  Wire.requestFrom(EEPROM_I2C_ADDRESS, 1);
  if (Wire.available()) {
    return Wire.read();
  }
  return 0xFF;  // Return 0xFF if no data is available
}
