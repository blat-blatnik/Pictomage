# Room file format

This document describes the binary format in which all the rooms were stored. The format is little endian. The fields are stored sequentially.

```c
uint64_t  flags                              // Unused. Intended as a kind of versioning system, but is always 0.
char      nextRoomName[64]                   // The filename of the next room file to load when stepping onto a portal.
uint8_t   numTilesX                          // The horizontal size of the room in tiles.
uint8_t   numTilesY                          // The vertical size of the room in tiles.
uint8_t   tiles[numTilesY][numTilesX]        // Tile-kind codes.
uint8_t   tileVariants[numTilesY][numTilesX] // Tile-variant codes.
Vector2   playerDefaultPos                   // The position at which the player spawns when the room starts.
uint8_t   numTurrets                         // The number of turrets in the room.
Vector2   turretPos[numTurrers]              // Position of each turret.
float     turretLookAngle[numTurrets]        // Look angle of each turret, away from (1, 0) in radians. 
uint8_t   turretVariants[numTurrets]         // Turret-variant codes for each turret.
bool      turretIsDestroyed[numTurrets]      // For each turret, whether it starts off destroyed.
uint8_t   numBombs                           // The number of bombs in the room.
Vector2   bombPos[numBombs]                  // The position of each bomb.
uint8_t   bombVariants[numBombs]             // Unused. Bomb-variant codes for each bomb, but is always 0.
uint8_t   numGlassBoxes                      // Number of glass boxes in the room.
Rectangle glassBoxRects[numGlassBoxes]       // The bounding boxes of every glass box.
float     glassBoxRotations[numGlassBoxes]   // Unused. Glass box angle in radians, but is always 0.
uint8_t   numTriggerMessages                 // Number of trigger messages in the room.
Rectangle triggerMessageRects[numTriggerMessages]  // Activation areas for every trigger message.
bool      triggerMessageOnce[numTriggerMessages]   // Unused. For each trigger message, whether it will only trigger once.
char      triggerMessages[numTriggerMessages][128] // The text of every trigger message.
```
