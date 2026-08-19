- ~~Add menu glow effect~~
- ~~Add water tab onto svelte app~~
- Add goal system onto svelte app
- Add a way to view all these goals on the LVLGL app
- Draw plant being grown
 - Figure out what resolution it should be drawn in first
- Add in depth analysis on the Sleep and breathing trackers on svelte
- Ask shreesh to print out 
- Add instructional text to the home screen 
- Accent color for each tab in the svelte app should change according to the logo

## TAMAGOTCHI PAGE ON SVELTE
- This page will have 2 tabs - the actualy tamagotchi section which will be added later, and the goals and points section
- The points will be labelled as seeds instead of points
- The goals section will display the points that the user accumulates over time, from completing tasks and using the variety of features within the app. Before we implement the goal system, it would be good to have a comprehensive list of all the different ways points can be added
- THis goals section in of itself will have a few functions
    - Firstly the user should be able to see where their points came from that day. This should be communicated in a user friendly logs, with very easy to understand language 
    - And the user types up 3 custom goals, and they should be able to type these in at the start of every single day. Whenever the user completes these goals, they get a point reward, witha n additional award if you complete all three. The user should also bee able to have streaks
    - This leads me to the third feature - streaks. If the user is sleeping consistently, drinking water consistently, consistenly completing breathing activities, etc, they build a streak. Each of these streaks should be individually displayed 
## NUANCE FOR THE GOAL SYSTEM:
- ~~Whenever points are added, the reasons for these points being added should be kept track of in a log of some kind - the user should be able to see what behaviours that they are doing is being rewarded~~ (backend done: 50-event history ring, persisted, broadcast via `points_sync`/`points_earned`; web-app rendering still pending)
- ~~For the water tracker, if the user reduces the amount of watre they have had that day, then the points should reduce as well~~
- ~~When the user hits their water goal for the day, then they should get a big boost of points~~
- ~~For the breathing tracker, the points should scale by a small amount based on the number of cycles per session~~
- ~~for the breathing tracker, if the user has already done a breathing cycle within a half an hour window, then they should get a much reduced amount of points~~
- ~~When you click the awake button, if you have had any amounut of sleep at all, you get a very small amount of points, to encourage the user to at least keep track of their sleep~~
    - ~~If they have more than 3 hours, they get more, if they have 6 hours they get a little more, if theyhave more than 7 hours theyhave more, if they have more than 8 hours, they get substantially more~~ (tiers: <3h=+5, 3h+=+10, 6h+=+15, 7h+=+25, 8h+=+50)
- ~~If the user removes a water they have added, then it should remove the points associated with this (per-glass refund done)~~
    - ~~Not done: claw back the +20 water-goal bonus when the count drops back below goal~~ (now done — bonus is reclaimed and re-earnable)
- ~~If the user increments their water counter while in a session break, they get extra points~~ (done: +10/glass during an active break)
    - ~~To implement this one, you also need to implement A. under the BUGS heading~~ (done — timer keeps running in background)


## BUGS
- ~~A. Can you modify the timer app, so that if you have a timer running, and you NAVIGATE TO THE MENU OR SWITCH SCREENS, THE TIMER IS STILL RUNNING in the background?~~
- ~~The timer states on the screen dont get saved properly 
    If i select a timer, i can select whichevr timer i want, and the timer tracks fine. However, if i leave this timer and select a new one, and THEN click play on this new one - it doesnt work. IT still think i'm on the old timer. WHen i refresh the timer display screen~~
- The timer edit screen isnt centered properly
- ~~The timer Carosel is still very glitchy, and mvoes with very low FPS~~
- The graph on the sleep tracker doesnt display anything on the LVLGL app
- ~~The "AWAKE" button is currently locked onto green - change it to the palette color of the Sleep LVLGL screen~~
- if the to do list entry on LVLGL has multiple lines, the centering doesn work properly

