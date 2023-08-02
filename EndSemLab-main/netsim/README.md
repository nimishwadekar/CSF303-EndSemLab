This is the main network simulator. Port specifications are as follows:

- `10000` - `1000n` : Ports representing `n` different links from the user router into the network.
- `22222` : Port that sends error messages in case any error happens. Connections to this handled by the user API.

## `tests.json`
The file `tests.json` contains an array of test cases that the simulator will use to test the user.

A test case is a JSON object with the following fields:
```
{
    "send_link": 2,
    "send_packet": {
      "src": 3,
      "dest": 7,
      "ttl": 12,
      "type": "data",
      "flags": [],
      "seq_no": 34,
      "payload": "packet1"
    },
    "recv_link": 2,
    "recv_packet": {
      "src": 7,
      "dest": 3,
      "payload": "1tekcap"
    }
}
```  
### Test Case Specification:
- `send_link`: The link the packet is sent on.
- `send_packet`: The packet to send.
- `recv_link` and `recv_packet`: The expected link and packet. (If not mentioned, the router is expected to drop the packet)

NOTE: The `type` field of the packet determines the data type of the `payload` field.
If `type` is `"cmd"`, it is a command packet and `payload` must be a command payload object.
If `type` is `"data"`, it is a data packet and `payload` can either be a UTF-8 string or an array of bytes.

NOTE: The `entries` field of a command payload object is an array of 2-element (destination and link weight) arrays.

NOTE: For `"recv_packet"`, the only fields needed to be mentioned are the ones that will change from `"send_packet"`.

NOTE: Packets being sent and received on the same link are implicitly assumed to be operation tests (tests for the user application and not the user router) and not routing tests.