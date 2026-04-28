# CBF+ (WIP)

## What does it do?

This mod completely reworks the discrete tick-by-tick method Geometry Dash uses for physics. Using kinematic formulas, the mod can interpolate the player's position and velocity at any point in time. The main impact of this is to be able to separate ticks per second and input refresh rate. Meaning the physics can update at some independent rate from the input rate allowing for customizable precision.

## Why did I make this mod?

Click Between Steps and Click Between Frames both use a method called "tick splitting" to allow
sub-tick inputs. When an input lands between two ticks, the tick gets split into two or more smaller steps that run independently.  
(Vanilla Click Between Steps is slightly different as it forces no more than one split per tick)

For example, a cube at 1x speed falling under gravity at 240 TPS:

- Gravity per tick: `0.958199 * (54/240) = 0.215594775` vels/tick
- Starting velocity: 0 vels

**Vanilla (one full tick):**  
`v = 0 - 0.215594775 = -0.215594775`  
`dy = 0.225 * (-0.215594775) =` **-0.048508824375 units**

****Tick split at 30% (two subticks of 0.3 and 0.7):****  
**Step 1 (30%):**  
`v = 0 - (0.215594775 * 0.3) = -0.0646784325`  
`dy = 0.225 * 0.3 * (-0.0646784325) = -0.00436579419375`  
**Step 2 (70%):**  
`v = -0.0646784325 - (0.215594775 * 0.7) = -0.2158418575`  
`dy = 0.225 * 0.7 * (-0.2158418575) = -0.03399509255625`
**Total dy = -0.03836088675 units**

Those clearly don't match. This comes from gravity being applied to velocity before
displacement in each subtick. Splitting one tick into two smaller ones changes the
intermediate velocity, which changes the total displacement. The error varies with split position and can accumulate. Note that in actuality, velocity is rounded to 3 decimal places. For the sake of "simplicity" I decided to use the full non-rounded values in the example. In game the disparity may be even worse due to the lost precision.

This mod takes a different approach: instead of splitting ticks, it evaluates the player's position
using a continuous formula that reproduces vanilla's physics exactly at every tick boundary.
No need for tick splitting since ticks no longer handle inputs.  
Inputs fire at the nearest input check (customizable in mod settings) without splitting anything since the formula gives the correct
position for any point in time.  
The result is sub-tick input precision with zero physics deviation
from vanilla.

## Future plans

- Platformer support
- 2.1 mode (use Velocity UnRounding and Enable 2.1 Subframes to replicate 2.1 physics. 2.1 exclusive bugs not yet available.)
- Non-Windows support
- Botting support

## Credits

Thanks to syzzi for the original CBF idea, this was obviously heavily inspired by it.

## Contact

Recreating GD physics isn't the easiest of things to do so don't be surprised if you find any bugs. Feel free to message me on discord at @ch.zzy if you have any issues though.
