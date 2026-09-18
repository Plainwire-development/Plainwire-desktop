# QA checklist
## for maintainers.

Before publishing a build:

- `./scripts/verify.sh` passes.
- A clean `./build.sh` succeeds with warnings treated as errors.
- Login survives a restart.
- DMs, servers, message history, reactions, uploads and calls match plainwi.re.
- Long chats scroll smoothly and resizing does not trail behind the window.
- 100%, 110%, 125% and 150% desktop scaling remain usable.
- Settings work at narrow and wide desktop window sizes.
- Reaction chips and the reaction picker remain clickable and update normally.
- Microphone, camera and screen-share permission prompts are understandable and persist correctly.
- Screen/window sharing starts from the native source picker.
- Downloads prompt for a destination and show completion/failure feedback.
- JavaScript alerts do not create blocking debug-style windows.
- A renderer crash can recover with a reload instead of leaving a dead window.
- Notifications focus Plainwire correctly.
- Closing to tray keeps realtime state active.
- A second process forwards its URL to the existing process.
- `plainwire://dm/...`, channel, server, profile and wire/invite links route correctly.
- External HTTP/HTTPS/mail links open outside Plainwire.
