- ~~When I try to add a to-do to the svelte app, it doesnt update properly.~~
- ~~The user shouldnt be able to start and stop timers through the svelte app - they might be able to MONITOR timers, but not actually start and stop them.~~
- ~~The time listed below the timer presets on the svelte apps arent actually accurate. Make sure this displays the times of each preset~~
- ~~The horizontal carousel on the LVLGL app has animations that move much too slow - increase the animation sped~~
- ~~The Sleep panel doesnt update properly~~
- ~~The to do list works great, but the svelte app doesnt sync with the lvl gl app, unless if i do a full refresh, disconnect and reconnect~~
- ~~You shouldnt be able to start/stop the breath cycle through the svelte app - only view statistics, and change the preset selected.~~
- ~~The breathing info that the we server recieves is entirely placeholder information it seems - can you go ahead and read the LVL GL implementation and change it so that it actually relates to what the app has teh acapabillity to do at the moment?~~
- ~~Add graphs to the sleep and breathing panels - want to see the amoutnt of hours slept per night, and the amount of cycles completed per day. These graphs should be constructed through the logs coming through from the LVLGl app. The svelte app should be able to keep track of these lgos no matter what~~
~~E (4188) spi_master: spi_master_deinit_driver(372): not all CSses freed~~
~~W (4188) SD_Card: spi_bus_free returned 259 (may already be free)~~
~~E (4198) spi_common: spi_bus_initialize(895): SPI bus already initialized.~~
~~E (4208) SD_Card: SPI bus init failed: 259~~
~~W (4208) Persistence: SD card not available, running without persistence~~
~~I have the sd card plugged in why isnt it avalibale? The sd card is empty~~

- ~~The UI for the breathing app is a little bit broken - the slepe app and the breathig app are on teh same page, as well as the breathing app graph doesnt update. Instead of being a fixed sat,sun mon tuesday ..., can you make it so you can look at the past month/week/3days~~
- ~~Do the same fro the sleep app, but make sure it actually moves~~
- ~~when i create a new timer preset on the lvl gl app, instead of making a new preset, it just replaces an existing one with "edited timer"~~
- ~~make sure the timer data is updated on the svelte app with the sync~~
- ~~The timer app can see when a timer is active, but it doesnt actually track the current state of the timer~~
- ~~THe timer app should also be able to diferentiate btween pomodoro and timer when its displaying its tracking~~
- ~~There should be some analytics for the pomodoro~~
- ~~THe lvl gl app currently doesnt accurately track the reall time, nor do i think it knows the real date~~
- ~~Can you seed some fake data for sleep and breathing for the past 5 days, but DONT do anything for the current day~~
- ~~Based on what I have setup for the breathing app and the sleep app, can you make a LVLGL screen for the water app?~~
- ~~Can you implement the png logos in /assets into the svelte app?~~