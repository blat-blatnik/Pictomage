# Room file format

```c
uint64_t flags // These are completely unused.
char nextRoomName[64]
uint8_t numTilesX
uint8_t numTilesY
uint8_t tiles[numTilesY][numTilesX]
uint8_t tileVariants[numTilesY][numTilesX]
Vector2 playerDefaultPos
uint8_t numTurrets
Vector2 turretPos[numTurrers]
float turretLookAngle[numTurrets]
uint8_t turretVariants[numTurrets]
bool turretIsDestroyed[numTurrets]
uint8_t numBombs
Vector2 bombPos[numBombs]
uint8_t bombVariants[numBombs]
uint8_t numGlassBoxes
Rectangle glassBoxRects[numGlassBoxes]
float glassBoxRotations[numGlassBoxes]
uint8_t numTriggerMessages
Rectangle triggerMessageRects[numTriggerMessages]
bool triggerMessageOnce[numTriggerMessages]
char triggerMessages[numTriggerMessages][128]
```