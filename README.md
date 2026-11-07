# Moon Sweepers Trainer

Petit trainer pour **Moon Sweepers** (Steam, Unity IL2CPP) qui débloque le **menu de développement caché** du jeu : un onglet **Dev** dans *Pause > Paramètres* (ajout d'argent, poussière transportée / dans les véhicules, terminer des zones, poser des robots, noclip…).

Rien n'est modifié sur le disque : le trainer patche deux tests en mémoire dans `GameAssembly.dll` pendant que le jeu tourne.

## Utilisation

1. Lance Moon Sweepers.
2. Lance `MoonSweepersTrainer.exe` et appuie sur **1** (Activer).
3. Charge ta partie (ou recharge-la si elle était déjà ouverte) : le menu pause n'est construit qu'au chargement.
4. *Pause > Paramètres* → onglet **Dev**.

Touches : **[1]** activer · **[2]** désactiver · **[Q]** quitter (retire les patches).

En ligne de commande : `MoonSweepersTrainer.exe on` / `MoonSweepersTrainer.exe off`.

Les patches disparaissent quand le jeu se ferme : relance le trainer à chaque session.

## Compilation

Avec MSVC (Developer Command Prompt x64) :

```
cl /nologo /O2 /EHsc /MT trainer.cpp /Fe:MoonSweepersTrainer.exe psapi.lib
```

## Fonctionnement

Le jeu contient `MoonCleaners.MoonDevTools.get_Enabled()`, qui ne renvoie `true` que dans l'éditeur Unity ou une build de debug. Le trainer écrit deux patches :

| Cible | RVA | Original | Patch |
|---|---|---|---|
| `MoonDevTools.get_Enabled` | `0x469b10` | `48 83 EC` | `B0 01 C3` (`mov al,1; ret`) |
| `MoonPauseMenu.Build` (test `Debug.isDebugBuild` inliné) | `0x46d6cb` | `E8 80 14 5E 01` | `B0 01 90 90 90` |

Les adresses correspondent au **build Steam 26317497**. Avant d'écrire, le trainer vérifie les octets d'origine. S'ils ne correspondent pas (mise à jour du jeu), il refuse de patcher au lieu de risquer un crash : il faut alors retrouver les nouvelles adresses.

## Avertissements

- À utiliser **en solo**. Le jeu a un mode coop (« Crew ») et les actions de dev affectent probablement la partie des autres joueurs.
- Les outils de dev peuvent abîmer une sauvegarde : fais-en une copie avant.
- Projet non officiel, sans lien avec les développeurs du jeu.
