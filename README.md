# LGTVDeviceListener: switch LGTV inputs based on Windows device changes
*Brought to you by [Etienne Dechamps][] - [GitHub][]*

**If you are looking for executables, see the [GitHub releases page][].**

## Description

LGTVDeviceListener is a small Windows tool that makes it possible to switch
inputs on a networked WebOS LG TV (e.g. G1), in response to a hardware device
appearing or disappearing from the system.

This is especially useful to build a poor man's [KVM switch][], by combining
LGTVDeviceListener with a USB switch. In this way the TV can be made to follow
the presence of the USB device and automatically switch video inputs in lockstep
when the USB device is switched.

## Requirements

- Windows Vista or greater
- A WebOS LG TV that is accessible over the network (wired or WiFi)

## How to use

LGTVDeviceListener is a command-line application. Pass `--help` to see all
available options.

### Preparation

After downloading the executable, run it with `--url` first. For example, if
your TV is listening on `192.168.42.42`:

```
LGTVDeviceListener.exe --url ws://192.168.42.42:3000
```

This will do two things:

1. A new client key will be registered with the TV if not done already. Your TV
   will ask you to confirm the registration.
2. LGTVDeviceListener will then start listening for device events and log them
   to the console.

While listening, add and remove the device you want to use as the trigger. (For
example, if it's a USB device, disconnect/reconnect it.) You will see
LGTVDeviceListener logging "Device added" and "Device removed" events. Take a
note of the *device name* - the identifier that normally starts with `\\?\`.

In some cases there could be many devices shown, especially for composite
devices such as USB hubs. In that case you can pick one at random, though it
usually makes the most sense to use the device with the shortest name because
that's typically the "root" device (i.e. the hub itself in the case of a USB
hub).

Once that's done, kill LGTVDeviceListener by hitting CTRL+C and move on to the
next step.

### Running as a console application

For quick test runs you can run LGTVDeviceListener directly as a console
application. You will need to specify the following parameters on the command
line:

- `--url`: see previous section.
- `--device-name`: the device name you chose in the previous section.
- `--add-input`: the input that the TV should switch to when the device is
  *added*.
  - This can be anything that the TV's `switchInput` API will accept. For
    example: `HDMI_3`.
- `--remove-input`: the input that the TV should switch to when the device is
  *removed*.

Note that you don't have to specify both `--add-input` and `--remove-input` - it
is possible to specify only one of them.

Here's an example of a complete LGTVDeviceListener command line:

```
LGTVDeviceListener.exe --url ws://192.168.42.42:3000 --device-name \\?\USB#VID_1234&PID_5678#foo&1&2#{bar} --add-input HDMI_1 --remove-input HDMI_2
```

With the above command, the TV at `192.168.42.42` will be switched to the HDMI 1
input when the `\\?\USB#VID_1234&PID_5678#foo&1&2#{bar}` device appears, and
will be switched to the HDMI 2 input as soon as the same device disappears from
the system.

This will happen for as long as LGTVDeviceListener is running. If you'd like to
make this setup permanent, see the next section.

### Running as a Windows service

If you'd like LGTVDeviceListener to run quietly in the background without having
to run it as a console application, the cleanest way is to set it up as a
Windows service:

1. Move `LGTVDeviceListener.exe` to a permanent location that is accessible
   system-wide (e.g. in `Program Files (x86)`).
2. Open a command prompt as Administrator.
3. Run LGTVDeviceListener using the same command you used in the previous
   section, but add the `--create-service` option.

For example:

```
LGTVDeviceListener.exe --url ws://192.168.42.42:3000 --device-name \\?\USB#VID_1234&PID_5678#foo&1&2#{bar} --add-input HDMI_1 --remove-input HDMI_2 --create-service
```

LGTVDeviceListener will set itself up as a new Windows service called
`LGTVDeviceListener` and start it. From this point on, LGTVDeviceListener will
be operating silently in the background and will start with Windows. The
parameters used for the service are the ones you included with the
`--create-service` command.

When running as a service, messages that are normally logged to the console,
including any errors, will be logged to the Windows Event Log (*Application*)
instead. You can use the Windows Event Viewer to see them.

To delete the service, run the following commands:

```
sc.exe stop LGTVDeviceListener
sc.exe delete LGTVDeviceListener
```

You can then re-create the service with different command-line parameters if you
so wish.

## See also

- [LGTVCompanion][] is a tool for automatically turning LG TVs on and off in
  response to Windows power events.
- [bscpylgtv][] can be used to send arbitrary commands to LG TVs.

[bscpylgtv]: https://github.com/chros73/bscpylgtv
[Etienne Dechamps]: mailto:etienne@edechamps.fr
[GitHub]: https://github.com/dechamps/LGTVDeviceListener
[GitHub releases page]: https://github.com/dechamps/LGTVDeviceListener/releases
[KVM switch]: https://en.wikipedia.org/wiki/KVM_switch
[LGTVCompanion]: https://github.com/JPersson77/LGTVCompanion
