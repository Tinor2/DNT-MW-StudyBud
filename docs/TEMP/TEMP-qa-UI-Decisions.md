## Color Palette and Theme
### 1. Colour palette direction
 I'm not too sure exactly what i want for this, so can you make the code so that it is very simple to change the color palette of the entire website, preferably accesible through a single file? 
However, lets go with the lavender and sage green direction
### 2. Accent Color
 Same as above response. However, for now, lets have a single unified color for everything
### 3. Typography 
 I dont really like either the montserrat direction, or the rounded option, but out of the two id lean more towards the rounded. Is it possible for me to download a new font library onto the esp32 and use that instead, or is that unneccesarily comlpex? I dont mind comprimising
### 4. Web app typography
 Probably a complementary one? Again, if i can download a new font library from somewhere like google fonts, that would be preferable
### 5. Icon style for the menu wheel
 Definetely would go with the custom pixel art icons. However, what would be the rough resolution these icons should be in?

## Disaply Behaviour
### 6. Idle mode activation
 This should definetely be configurable. The user should also be able to switch to the idle mode via an app on the menu bar
### 7. What does the display show during idle beside the background?
 I hope that i can have a clock on the background, maybe also session indicators. However, these should be somewhat minimal, but before you go ahead with that, I will give you some concept designs to get a better idea - put a pin in this.
### 8. Wake from idle
 Any encoder action should wake it up.
### 9. Status bar
 Maybe dont have a persistent status bar. Other than time, there isnt anything that i NEED the user to be seeing at all times - the wifi connection should be relatively simple, and visible in the settings menu,I might NOT use a battery - so the device will always just need to be plugged in (this is left to be seen though). So no, i dont think theres a need for the staus bar.
### 10. Menu Wheel layout
 That looks about right, but over time i might add some more apps to this list. I would like to add a breathing/calmness app, tamagotchi features, etc. I will need help brainstorming certain parts of this product - importantly, I have been misguiding you a little bit - the primary purpose of the app should NOT be just a study timer, rather something to help manage stress. The to dos, and timers are still going to remain as a feature, but they arent the FOCUS. Because of this, I probably want to end up with a lot more features (probably 10-12)
 So far the features that I want to implement are 
    - Home (A clock NOT a timer - this will also tell the user general information that might be useful - this is where you could put the wifi and other thigns that you mentioned could go in a status bar. These should be very un-obtrusive)
    - Timer (within this app there would be two pages - a list of presets and then the timer itself. One of these presets will be bold and maybe in a different color, and is called Pomodoro - you can modify the settings of this timer in the web app.)
    - To dos
    - A water tracker 
    - Meditation/Breathing excercize
    - Sedentary tracker?
    - Idle background
    - Tamogotchi feature
    - Settings 
 I plan to add more featuers on top of this. Is the idea of using a menu wheel becoming unresasonable?
## Web App Navigation
### 11. Nav bar position
 Probably the collapsible sidebar and hamburger menu option
### 12. Does the web app have a landing/home page
I definetely want to have a landing page. Note that i also want an login page
## Task Management Details
### 13. How does the user add task?
 The microphone would be a really nice feature, but lets just stick to keyboard only, and maybe think about adding tasks via voice in the future
### 14. Task due dates/priorities
 Maybe not worry about due dates, but we could have small little priority tags (high-low importance). However, again the to do list and study isnt the focus of this product.
### 15. How many tasks can the display show at once?
 2-3 tasks should be ok honestly
## Timer details
### 16. Session count display
 Maybe just the current phase. Remember ALL the timers wont function like the pomodoro, only some of them.
### 17.Break timer behaviour
 A different color would be nice, but it should maybe just be the progress bar and the timer text color that changes, notthing TOO drastic
### 18. Autostart breaks
 I would like the user to press the encoder to start breaks. Note that we do have a speaker as another peripheral, so the speaker should make a noise when this happens. The speaker should also be used in other apps like the breather and the to do, and the water tracker
## Water Tracker
For the water tracker, water Llama is an inspiration - hopefully the potential mascot i design will be in this app
### 19. Default daily goal
 This should be configurable through the app, but also have a default goal
### 20. Water Reminder
 The display showing an icon pulsing feels like a good idea. Maybe we can have a notification panel thats accesible, so other apps can also push reminders through there? 

Can you commit all of these recent changes/discussions into their respective markdown files, either in 






