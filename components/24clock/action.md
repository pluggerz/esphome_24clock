On startup the director sends the `DIRECTOR_ONLINE` message. 
Note, the performer derives its id from the source_id of the action:
*  If the source is the director (`SRC_MASTER`), then `performer_id:=0`, 
* otherwise it is `performer_id := source_id + 1`. The `reserved` field contains an unique random number to identify the director, so the performer can ignore duplicate messages.

The performer goes into in reset action. Actually waiting, for the `DIRECTOR_ACCEPT` message which contains the baudrate for the uart. The performer forwards the message with its `performer_id` as `source_id`.

Once, the  director receives DIRECTOR_ACCEPT back, then it will send the offsets 


