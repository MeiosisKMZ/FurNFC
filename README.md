# FurNFC ASCII

App externe Flipper Zero pour lire un tag Mifare Classic directement et pour
decoder aussi un dump `.nfc`.

## Ce que fait l'app

- Lit un tag Mifare Classic directement depuis le Flipper
- Boucle en attente sur `Lecture NFC...` jusqu'au tag ou jusqu'a `Back`
- Tente les cles par defaut `FF FF FF FF FF FF` sur tous les secteurs
- Permet aussi de saisir une cle Mifare perso en hex
- Permet aussi de choisir un autre fichier `.nfc`
- Ignore automatiquement les blocs trailer de secteur (`FF FF FF ...`)
- Affiche uniquement les caracteres ASCII lisibles dans une UI scrollable

## Structure

- `application.fam` : manifeste FAP compatible Unleashed/OFW
- `furnfc_ascii.c` : code de l'app
