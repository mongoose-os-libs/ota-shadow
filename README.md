# OTA firmware update via the AWS IoT device shadow


## Overview

This library provides a way to update firmware Over-The-Air via the
AWS IoT device shadow mechanism.

It works by observing the `ota_url` entry in the device shadow. That entry must
be an URL with the valid Mongoose OS firmware, which is a .zip file.
When that entry is changed, this library downloads a firmware from that
URL and kicks off an OTA update. The sequence of actions is as follows:

- Receive shadow delta for `ota_url` - a new `ota_url` value
- Compare `ota_url` with file on flash. If it is the same as new value, stop
- If it is different, save new `ota_url` locally on flash
- Report new `ota_url`
- Trigger an OTA by downloading the .zip
- During the process, report numeric `ota_code` and string `ota_message`
  entries which indicates the status of the OTA in nearly real time
- On any failure, stop with the failure `ota_message`
- On success, reboot to the new firmware
- After reboot, commit the new firmware after the successful AWS IoT handshake
- If AWS IoT handshake does not happen during 5 minutes (the default commit
  timeout), rollback to the old firmare
- OTA failure keeps the `ota_url` delta uncleared

## Screenshots

Build and flash the https://github.com/mongoose-os-apps/ota-aws-shadow app,
start the console, and provision to AWS IoT. Open the shadow GUI for the
device.

Upload built firmware to the
[Mongoose OS dashboard](https://mongoose-os.com/docs/overview/dashboard.html) 
and copy the firmware URL.

![](img1.png)

Create `ota_url` entry in the `desired` shadow state, paste the firmware URL
as a value. Save shadow. That triggers an update:

![](img2.gif)

When it successfully finishes, it reboots and commits the firmware after the
AWS IoT handshake:

![](img3.gif)
