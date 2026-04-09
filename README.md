# FurNFC ASCII

External Flipper Zero app to directly read a Mifare Classic tag and also decode a `.nfc` dump.

## What the app does

- Reads a Mifare Classic tag directly from the Flipper  
- Loops on `Reading NFC...` until a tag is detected or `Back` is pressed  
- Tries the default keys `FF FF FF FF FF FF` on all sectors  
- Also allows entering a custom Mifare key in hex  
- Also allows selecting another `.nfc` file  
- Automatically ignores sector trailer blocks (`FF FF FF ...`)  
- Displays only readable ASCII characters in a scrollable UI  

## Structure

- `application.fam`: FAP manifest compatible with Unleashed/OFW  
- `furnfc_ascii.c`: app source code
