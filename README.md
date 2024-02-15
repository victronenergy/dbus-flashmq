dbus-mqtt-flashmq-plugin
=========

A plugin that turns FlashMQ into a dbus aware MQTT broker. It also supports receiving requests to change values on the local D-Bus. It's a faster replacement for the original Python dbus-mqtt. And, because it integrated with FlashMQ's event loop, it can coordinate dbus and mqtt activity as efficiently as possible.

FlashMQ with this plugin runs as a service since [Venus OS](https://github.com/victronenergy/venus/wiki) version 3.20. To enable the MQTT service, go to 'Settings -> Services -> MQTT' on the menus of the GX device.

Dbus-flashmq runs inside FlashMQ running on the GX device itself. That broker will be
accessible on the local network at TCP port 1883 and/or 8883 (depending on your security settings). Furthermore, MQTT traffic can be configured to be forwarded to the central Victron MQTT broker (see [Determining the broker URL for a given installation](#determining-the-broker-url-for-a-given-installation)), which allows you to monitor and control your CCGX over the internet. You'll need your VRM credentials to access this broker. See 'Connecting to the Victron MQTT server' below.




Support
-------
Please don't use the issue tracker of this repo.

Instead, search & post at the Modifications section of our community forum:

https://community.victronenergy.com/spaces/31/mods.html

Building
--------
Once the Venus SDK is active, it's a simple matter of:

```sh
mkdir build
cmake [-DCMAKE_BUILD_TYPE=Release] /path/to/project/root
make
```




Notifications
-------------

When a value on the D-Bus changes, the plugin will initiate a publish. The MQTT topic looks like this:

```
N/<portal ID>/<service_type>/<device instance>/<D-Bus path>
```

* Portal ID is the VRM portal ID associated with the CCGX. You can find the portal ID on the CCGX in
  Settings->VRM online portal->VRM Portal ID. On the VRM portal itself, you can find the ID in Settings
  tab.
* Service type is the part of the D-Bus service name that describes the service.
* Device instance is a number used to make all services of the same type unique (this value is published
  on the D-Bus as `/DeviceInstance`).

The payload of the D-Bus value is wrapped in a dictionary and converted to json. 

Unlike the previous Python implementation, this plugin no longer causes messages to be retained by the broker, either locally or on the internet. There are fundamental problems with retained messages, that are solved by simply requesting the current topics as needed. See the [chapter on keep-alives](#keep-alive) and [backwards compatability](#migrating-from-previous-versions-and-backwards-compatability) below.

As an example of a notification, suppose we have a PV inverter, which reports a total AC power of 936W. The topic of the MQTT message would be:

	Topic: N/<portal ID>/pvinverter/20/Ac/Power
	Payload: {"value": 936}

The value 20 in the topic is the device instance which may be different on other systems.

There are 2 special cases:

* A D-Bus value may be invalid. This happens with values that are not always present. For example: a single
  phase PV inverter will not provide a power value on phase 2. So /Ac/L2/Power is invalid. In that case the
  payload of the MQTT message will be `{"value": null}`.
* A device may disappear from the D-Bus. For example: most PV inverters shut down at night, causing a
  communication breakdown. If this happens a notification will be sent for all topics related to the device.
  The payload will be empty (zero bytes, so no valid JSON).

If you want a roundup of all devices connected to the CCGX subscribe to this topic:

```sh
mosquitto_sub -t 'N/<portal ID>/+/+/ProductId' -h '<ipaddress/hostname>' -v
'N/<portal ID>/+/+/ProductId'
```

And then send a keepalive to refresh all topic values (this is described in more detail in the [keep-alive chapter](#keep-alive) below):

```sh
mosquitto_pub -t 'R/<portal ID>/keepalive' -m '' -h '<ipaddress/hostname>'
```

This also is a convenient way to find out which device instances are used, which comes in handy when there are
multiple devices of the same type present.




Write requests
--------------

Write requests can be sent to change values on the D-Bus. The format looks like the notification, however instead of an `N`, the topic should start with a `W`. The payload format is identical.

Example: on a Hub-4 system we can change the AC-In setpoint with this command line, using `mosquitto_pub`:

```sh
mosquitto_pub -h '<ipaddress/hostname>' -t 'W/<portal ID>/vebus/257/Hub4/L1/AcPowerSetpoint' -m '{"value": -200}'
```

The device instance (in this case 257) of a service usually depends on the communication port used the
connect the device to the CCGX, so it is a good idea to check it before sending write requests. A nice way to
do this is by subscribing to the broker using wildcards.

For example:

```sh
mosquitto_sub -v -h '<ipaddress/hostname>' -t 'N/<portal ID>/vebus/+/Hub4/L1/AcPowerSetpoint'
```

will get you the list of all registered Multis/Quattros (=vebus services) which have published `/Hub4/L1/AcPowerSetpoint` D-Bus path. You can pick the device instance from the topics in the list. As before, this does require a topic refresh by sending a keep-alive. See the [chapter on keep-alives](#keep-alive).




Read requests
-------------

A read request will force the plugin to re-read the current value from the dbus and send a notification
message of a specific D-Bus value. Again the
topic is identical to the notification message itself, except that the first character is a `R`. Wildcards
in the topic are not supported. The payload will be ignored (it's best to keep it empty).

Example: to retrieve the AC power of our favorite PV inverter we publish (with an empty payload):

```sh
mosquitto_pub -h '<ipaddress/hostname>' -t 'R/<portal ID>/pvinverter/20/Ac/Power' -m ''
```

The Venus device will reply with this message (make sure you subscribe to it):

```
Topic: N/<portal ID>/pvinverter/20/Ac/Power
Payload: {"value": 926}
```

Normally you do not need to use read requests, because most values are published automatically as they
change. For values that don't change often, most notably settings (`com.victronenergy.settings` on D-Bus),
you will have to use a read request to retrieve the current value.

You can also do a read request on sub-paths, like `R/<portal ID>/solarcharger/279/Dc`. This results in updates of the underlying paths:

```
N/<portal ID>/solarcharger/279/Dc/0/Voltage {"value":20.43000030517578}
N/<portal ID>/solarcharger/279/Dc/0/Current {"value":0.0}
N/<portal ID>/solarcharger/279/Dc/0/Voltage {"value":20.43000030517578}
```

Note that this is different from the previous API, which replied on `N/<portal ID>/solarcharger/279/Dc` with a serialized json representation of the deeper topics.




Keep-alive
----------

In order to avoid a lot of traffic, there is keep-alive mechanism. It works slightly differently from dbus-mqtt shipped in earlier versions of Venus.

To activate keep-alive, send a read request to `R/<portal ID>/keepalive` (or the legacy `R/<portal ID>/system/0/Serial`). It will send all topics it has, whether the system is alive or not. This is the replacement for retained messages as used by the Python dbus-mqtt. Because messages are no longer retained, if you are subscribing to a path like `N/<portal ID>/+/+/ProductId` to see all products, you must initiate a keep-alive request afterwards to see the values. See the next section.

Keep-alive timeout is 60 seconds. After it expires, `null` values are not published. See the next section about changes in behavior. 

When a keep-alive is received and all topics are published, the last topic will be `N/<portal ID>/full_publish_completed` with a payload like `{"value":1702459522}`. This topic signals that you have received all topics with their values, and can be a trigger for an application, to update the GUI, or go to the next state, etc.

You can specify `{ "keepalive-options" : ["suppress-republish"] }` to forgo sending all topics. Once you have woken up a system and continue to send keep-alives, you don't need a full update on all topics each time, and you can/should include the `suppress-republish` keepalive-option from that point on. A typical implementation would be to put the keep-alive with `{ "keepalive-options" : ["suppress-republish"] }` on a timer, and send keep-alives with empty payload explicitely, when the state of the program requires it.

For a simple command to activate keep-alive on a Linux system, see the [Notes](#notes) at the end of this section.




Migrating from previous versions and backwards compatability
----------------

With this new replacement for Python dbus-mqtt there are some changes:

#### 1) No more retained messages

Retained messages are messages that are kept by the MQTT server and are given to clients on subscription to an MQTT pattern. With dbus-flashmq, messages are no longer published as retained. This caused too much confusion about which topics were still valid and which weren't. Instead, when receiving a keep-alive, the system will simply republish all values. This allows existing clients to see all the topics once they connect and send the keep-alive, whether they are the first, second, third, etc. See the [section on keep-alives](#keep-alive).

A consequence of this, is that you will no longer immediately see a list of topics+values when you subscribe. You need to request all topic values with the keep-alive.

Another consequence, is that after keep-alive timeout, `null` values for all topics will not be sent. This was originally required to unpublish retained messages. Now, topics with `null` are only sent when devices disappear.

#### 2) No more selective keep-alive

Another change is that 'selective keep-alive' is, at least for now, not supported. Selective keep-alives was a mechanism to only keep certain topics alive, to reduce traffic and load. However, this effect was actually not achieved well, and with this new faster implementation, it's simply no problem to send all topics. 

#### 3) Reading a sub-tree at once

As described in the [Read requests](#read-requests) section, doing a read on a sub-tree like:

```sh
mosquitto_pub -t 'R/<portal ID>/settings/0/Settings' -m '' -h '<ipaddress/hostname>'
```

will give you an update on all sub-topics:

```
N/<portal ID>/settings/0/Settings/LEDs/Enable {"max":1,"min":0,"value":1}
N/<portal ID>/settings/0/Settings/Vrmlogger/Http/Proxy {"value":""}
N/<portal ID>/settings/0/Settings/Vrmlogger/Url {"value":""}
N/<portal ID>/settings/0/Settings/Vrmlogger/Logmode {"max":2,"min":0,"value":1}
N/<portal ID>/settings/0/Settings/Vrmlogger/HttpsEnabled {"max":1,"min":0,"value":1}
N/<portal ID>/settings/0/Settings/VenusApp/LockButtonState {"max":1,"min":0,"value":0}
etc
```

The previous implementation serialized all the answers as json.





Notes
----------------

An easy way to send a periodic keep-alive message without having to do it
manually is to run this command in a separate session and/or terminal window:

```sh
( first=""; while true; do if [[ -n "$first" ]]; then echo '{ "keepalive-options" : ["suppress-republish"] }'; else echo ""; fi ; first=true; sleep 30; done ) | mosquitto_pub -t 'R/<portal ID>/keepalive' -l -h '192.168.8.60'

```

You will need to install the mosquitto client package. On a Debian or Ubuntu
system this can be done with:

```sh
sudo apt-get install mosquitto-clients
```




Connecting to the Victron MQTT server
-------------------------------------

If the MQTT service is enabled, the CCGX will forward all notifications from the GX device to the Victron MQTT
servers (see the broker URL section for the correct URL). All communication is encrypted using TLS. You can connect to the MQTT
server using your VRM credentials and subscribe to the notifications sent by your GX device. It is also possible
to send read and write requests to the GX device. You can only receive notifications from systems in your own VRM
site list, and to send write requests you need the 'Full Control' permission. This is the default is you have
registered the system yourself. The 'Monitor Only' permission allows subscription to notifications only
(read only access).

A convenient way to test this is using the mosquitto_sub tool, which is part of Mosquitto (on debian linux
you need to install the `mosquitto-clients` package).

This command will get you the total system consumption:

```sh
mosquitto_sub -v -I 'myclient_' -c -t 'N/<portal ID>/system/0/Ac/Consumption/Total/Power' -h '<broker_url>' -u '<email>' -P '<passwd>' --cafile 'venus-ca.crt' -p 8883
```

You may need the full path to the cert file. On the CCGX it is in
`/etc/ssl/certs/ccgx-ca.pem`. You can also find the certificate in this repository as `venus-ca.crt`.

In case you do not receive the value you expect, please read the keep-alive section.

If you have Full Control permissions on the VRM site, write requests will also be processed. For example:

```sh
mosquitto_pub -I 'myclient_' -t 'W/<portal ID>/hub4/0/AcPowerSetpoint' -m '{"value":-100}' -h '<broker_url>' -u '<email>' -P '<passwd>' --cafile 'venus-ca.crt' -p 8883
```

Again: do not set the retain flag when sending write requests.

### Determining the broker URL for a given installation

To allow broker scaling, each installation connects to one of 128 available broker hostnames. To determine the hostname of the broker for an installation, you can either request it from the VRM API, or use an alghorithm.

For the VRM API, see the [documentation for listing a user's installations](https://vrm-api-docs.victronenergy.com/#/operations/users/idUser/installations). Each site has a field `mqtt_webhost` and `mqtt_host`. Be sure to add `?extended=1` to the API URL. So, for instance: `https://vrmapi.victronenergy.com/v2/users/<myuserid>/installations?extended=1`.

If it's preferred to calculate it yourself, you can use this alghorithm:

- the `ord()` value of each charachter of the VRM portal ID should be summed.
- the modulo of the sum and 128 determines the broker index number
- the broker URL then is: `mqtt<broker_index>.victronenergy.com`, e.g.: `mqtt101.victronenergy.com`
- the same goes for the websocket host: `webmqtt<broker_index>.victronenergy.com`

An example implementation of this algorithm in Python is:

```python
def _get_vrm_broker_url(self):
    sum = 0
    for character in self._system_id.lower().strip():
        sum += ord(character)
    broker_index = sum % 128
    return "mqtt{}.victronenergy.com".format(broker_index)
```

