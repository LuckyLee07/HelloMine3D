# CUA screenshot persistence — 2026-09-05

Result: **PASS for lossless file export**, not gameplay or AI-07 acceptance.

The current CUA documentation returns `Uint8Array` from `App.getScreenshot()`.
The earlier conclusion that no dedicated save helper implied no persistence
route was too strong. Original reports retain their historical missing-file
limitations; no earlier screenshot is reconstructed or relabelled.

## Observed pipeline

1. Bound `/private/tmp/HelloMine3D-0e0c569.app` using its exact path. A normal
   rendered world was visible. Its gameplay actions were not attributable to
   this executor, so the scene is a capture/export probe only.
2. Called `resumedGame.getScreenshot()`, keeping the returned bytes in memory.
   It returned JPEG/JFIF, 271352 bytes. Pure JavaScript calculated CRC32
   `25b19723` and encoded 361804 base64 characters. No alternate screen-capture
   API, game render hook, image generation or image re-encoding was used.
3. Through documented CUA methods, opened TextEdit. The initial getApp timed
   out; rebinding the same bundle succeeded without creating a second document.
   Clicked New Document, pressed `super+shift+t` for plain text, and used
   `paste(encoded, {format: 'text'})`. Observed state after each action.
4. Used `super+s`, the save-name field and `super+shift+g` to save locally as
   `build/goal-20260905/cua-screenshot-transfer.txt`. The save dialog confirmed
   the project directory rather than its initial iCloud location.
5. Python `base64.b64decode(text.strip(), validate=True)` recovered the bytes.
   Length271352 and whole-file CRC32 exactly matched the CUA-side values.
   Wrote `normal-window-cached.jpg`, calculated SHA256
   `777a5e099a1b5bc2b20ce3855684f0a685ac68c8b681205c2c874cc45f4b1e1a`.
   `view_image` successfully opened the saved file and showed the same scene.
6. Closed this saved TextEdit document. Closing exposed an Open dialog;
   Cancel click left it unchanged, then Escape returned `noWindowsAvailable`.
   No input was sent to the game during export; its window was left intact.

The file, byte identity and SHA256 are retained in `identity.json` and
`normal-window-cached.jpg`. Exact acquisition wall-clock time was not captured;
ordered CUA calls establish acquisition before export. Do not infer performance
or gameplay causality from this image.

## Reusable acceptance route

During an authorized normal-window run, retain each actual CUA screenshot byte
array with scenario, frame order, locale, UI scale, window size, package/binary
identity and capture time recorded at acquisition. After the run, encode those
unchanged bytes into one JSON/base64 text document and save through the same
normal editor route. Decode into image files using filesystem tools, verify
length/CRC against capture-side values, compute SHA256 and visually inspect.
Batch export after the run avoids switching focus during individual scenarios.
Only documented CUA handles operate the computer; file tools decode and verify
already captured data. Avoid printing large base64 values or broad unrelated
file-dialog contents into the transcript.

A technical export route is now proven. Full scenario image coverage still
requires new attributable normal-window captures. Held-key/relative input,
AI-06 readable-root isolation and real macOS audio remain separate limitations.
