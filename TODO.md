# TODO

* ✅ Progressive rendering stops at 51% - FIXED (race condition in camera preview logic)
* ✅ update draw only happens with mouse movement - FIXED (moved display updates to main thread)
* ✅ Raise max samples - FIXED (M key: 100→1000 samples, defaults: 1000→2000 samples)
* ✅ Save functionality issues - FIXED (can save COMPLETED or STOPPED renders, improved error feedback, preserve partial renders)
* output save is not a png when png chosen (maybe just file extension)
* Ghosting issue
* Camera control is jank (up Down are reversed for example and it's twitchy)
