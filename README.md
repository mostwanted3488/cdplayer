# CD Player FX

Plugin audio (VST3 Windows / Mac + Audio Unit Mac) qui transforme le vieux lecteur CD de Windows 95 en instrument : la piste défile au milieu, on la scratche, on la fige, on la fait sauter, on l'abîme…

par **@buyorbealoser**

## Télécharger

Chaque mise à jour du code est compilée automatiquement par GitHub. Les fichiers prêts à installer sont dans **[Releases → Latest](https://github.com/mostwanted3488/cdplayer/releases/latest)** :

| Système | Fichier |
|---|---|
| Windows (FL Studio, Ableton…) | `CDPlayerFX-Windows-VST3.zip` |
| Mac (Intel + Apple Silicon) | `CDPlayerFX-macOS-VST3.zip` et/ou `CDPlayerFX-macOS-AU.zip` |

## Installer

**Windows**
1. Dézippe `CDPlayerFX-Windows-VST3.zip`.
2. Copie le dossier `CD Player FX.vst3` dans `C:\Program Files\Common Files\VST3\`.
3. Dans FL Studio : *Options → Manage plugins → Find more plugins*, puis ajoute « CD Player FX » sur une piste du Mixer (ou sur le Master).

**Mac**
1. Dézippe les fichiers.
2. Copie `CD Player FX.vst3` dans `/Library/Audio/Plug-Ins/VST3/` (et `CD Player FX.component` dans `/Library/Audio/Plug-Ins/Components/` pour Logic / l'AU).
3. Le plugin n'est pas encore signé par Apple : ouvre le Terminal et tape
   ```
   sudo xattr -cr "/Library/Audio/Plug-Ins/VST3/CD Player FX.vst3"
   sudo xattr -cr "/Library/Audio/Plug-Ins/Components/CD Player FX.component"
   ```
4. Relance FL Studio et rescanne les plugins.

## Comment ça marche dans FL Studio

Le plugin est un **effet** : il traite le son de la piste où il est placé (ou de tout le mix sur le Master).

- **Zéro latence par défaut** : la waveform montre ce qui vient de jouer (tête de lecture à droite).
- **Lookahead** (Disc → Lookahead : 0,5 s, 1 s ou 2 s) : optionnel. Le plugin garde de l'avance pour afficher et scratcher ce qui arrive (tête de lecture au centre, comme dans la vidéo), mais FL Studio retarde alors tout le projet de cette durée. À éviter pendant que tu enregistres ou joues au clavier.
- **Mémoire** : les ~60 dernières secondes sont gardées. Scratch, cues, freeze, reverse et A–B jouent dedans.
- **Slip mode** (activé par défaut) : quand tu lâches un scratch, une cue ou un freeze, le son reprend pile là où serait le morceau → tu restes toujours calé sur le beat.
- **Grille** : calée sur le tempo et la position de FL Studio.
- **Speed** : comme le tempo est verrouillé par FL, Speed fait monter / descendre la hauteur comme une cassette (effet nightcore / chopped).

## Contrôles

| Action | Comment |
|---|---|
| Scratch | glisser la waveform |
| Freeze / stutter | clic droit maintenu (ou F) — plus tu cliques haut, plus la boucle est courte |
| Repeat A–B | Shift + glisser (Échap pour effacer) |
| Cue points 1–8 | pads ou touches 1–8 : 1er appui = poser, maintenir = jouer, Shift / clic droit = effacer |
| Rewind / avance rapide | maintenir ◀◀ ▶▶ (ou flèches) |
| Pause = tape stop | bouton lecture ou Espace |
| Effets rapides | Z skip · X rayures · C pile faible · V lo-fi · B mega bass · A anti-shock · H écouteurs · W room · T gate · Q reverse |
| Fenêtres | E effets · G égaliseur · P presets · S skin suivant · F1 aide |
| Shuffle / Reset | R / 0 ou bouton RESET |
| Dry / Wet | slider en bas à droite |

Skins : Classic, Winamp, Oscilloscope, LCD Discman, Media Player 98, Blueprint, Custom (View → Skin).

## Compiler soi-même

```
cmake -S . -B build
cmake --build build --config Release
```
JUCE 8.0.4 est téléchargé automatiquement. Un test du moteur sans interface existe : `cmake -S . -B build -DCDFX_TESTS=ON`.

La preview web d'origine est dans `preview/index.html`.
