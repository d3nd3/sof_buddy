menu-know-how

# Internals

Page inherits information from its parent

---

Singular MenuSystemClass -> Linked List of Menus

Menu -> Linked list of Frames

The initial frame starts out the full size of the page, but can be resized with keyword resize in `<stm` tag. 

Frames are named. Do not need to have content. Define areas of the screen.

Frame -> Linked List of RmfPages 

A Frame generally has 1 RmfPage, but can have more if using the `menu rmfPage targetFrame` command.

RmfPages = Linked List of Areas

There is a RmfPage viewing Stack, the RmfPage at the top of the stack is shown. 

---

The first global frame of the loaded Menu, uses the first RmfPage as loaded from the .rmf file to populate the Areas Linked List.

Any `<frame` that is specified in any RmfPage will always append to the Menu Linked list of Frames. In other words, Frames only become recursive and nested, if the cut keyword is used, which points Frames to each other in a parent child relationship.


---
# Guidelines

Should anything fail, a developer warning will be issued and the area will simply not appear.

All images use .m32 format.
All ghouls use .ghl format.

**The width, height of Area elements is using absolute screen pixels**

**but \<frame is using % of screen**
**this is a strong reason for why you would use frame over area with blanks**

---


The `<frame name frameWidth frameHeight` uses:

for the width:
frameWidth * 100/640
for the height:
frameHeight * 100/((1080/19290)*480)

To compute the % of the actual screen pixels it renders at/occupies.

---

Its important to note that frames do not care about hbr or br or layouts. They operate on a higher level.
The only rule that follow is that when they wrap, they do so to the left most side of the last auto (0) filled/stretched frame that caused the wrap.
This means that if there was a frame to the left of the auto (0) frame (HORIZONTALLY) of a fixed side, that frame's space would act like a space/margin
all the way vertically to the bottom (because new wrapped frames can never access that space).

You can only have a frame as a border top (vertically placed) by layers of completed horizontals. Because the next frame is always
placed to the right of the previous frame on the same vertical, by default. (until the 0, auto,stretch thing is completed.)

```xml

<frames exist outside of the <br> system

<br> stuff live within frames
```

---

```xml
<stm>
<defaults border 1 1 white>
<blank 16 100>
<defaults border 1 1 cyan><br><normal>
<br><blank 16 16>
<defaults border 1 1 green><center>
<blank 2 2><blank 16 16>
<defaults border 1 1 blue><right>
<blank 16 45><blank 16 16>
<defaults border 1 1 orange><right>
<blank 16 80><blank 16 16>
</stm>
```

```xml

layout change on a <br> line is literally:
reset of all local <br>, its like a clear, or typewriter back to top, so you can have many 'threads'/drawing attempts within the same HARD-BR <hbr> cluster.
everything above/before the <hbr> is cemented, and you can't access it again.
you are building little towers/sand castles, out of <br>, each <br> is of height 'current font height' ... eg. `<br> + <br> + <br> = 3x<br> sand castle.
layout change = clear the sand castle, but keep what was drawn from at the top of the previous sand-castle
```

```xml
Notice:
- The 'top' is defined by the last <hbr>
- <br> act like temporary stepping stones for the current layout change. Every-time a layout change, the <br> stacks are reset. So they are like 'local' variables.
- Area's still try to grow to the right
- <br> and <hbr> are the ways to grow vertically down-wards. Aswell as auto wrap when areas reach the right side of the frame.
- The first line is counted as a <hbr> line (not a <br line).
- <br> = weak and temporary
- If an area does not fit into the frames' total width, it will not render.
- Areas (and text, which is an area) will autowrap if not enough space on current line.
```

## Useful Qwirks to know
* exitcfg was annoying because it prints exec text - used onexit instead
* `return` is a wrapper around `menuoff`, handling extra checks; can prevent some pops/teardowns under singleplayer-like conditions.
* `noshade` is useful to prevent default auto tinting on the area (for example `list`/`ctext`).
* anything before `<stm>` or after `</stm>` is ignored.
* `backdrop` is primary full-background control. in many cut-frame layouts, `backfill` reads as frame-local/border-region fill
* RMF tint  use ABGR-style interpretation
* plain words are flow text; `<text ...>` is interactive/clickable area text.
* CAUTION: `<center>` layout treats each space separated word as a new line, then if you use double quote around your space separated text, if the length is longer than the remaining space in the frame, it will not be rendered.
* Seems wise to try to use 2 giant blank borders either side of the content, but they must be injected with resolution dependent pixels as: ((width_of_inner_frame_px - centered_content_size_px) / 2) . Then you have a fixed size area across resolutions. CAVEAT: hr element cant be used cos has inbuilt left
 
---

# Syntax

## Commands

menu `<menu>`  *This constructs a menu page or redisplays it if it has already been constructed. E.g. “menu main” brings up the main menu. If a frame name is specified, the page will be built in that frame.*
* frame

intermission `<menu> *Identical to the menu command, with the exception that the created page cannot be exited.*`

reloadall *Marks all pages in the stack to be reloaded when they are next visited. Used to change languages.*

refresh *Refreshes the topmost page. For example, if a displayed console variable has changed by another process, you will have to refresh the screen to see it in the menu.*

popmenu *Pops the most recent page off the top of the stack as long as there is a valid page behind it. If the most recent page was the only page on the stack, then the menu will only be popped if the console is enabled.*

killmenu *Identical to popmenu, but always pops the page irrelevant of the validity of the page behind or the availability of the console.*

return *Destroys the whole menu system provided there is an active game running in the background. I.e returns to the game.*

menuoff *Destroys the whole menu system including all pages currently on the stack.*

select `<cvar>` *Used to select items. Used exclusively with the selection area.*

checkpass `<password>` `<pass menu>` `<fail menu>` *Checks **password** against the parental lock password and displays the **pass menu** if they match or the **fail menu** if they don’t*

changepass `<oldpass>` `<newpass>` `<verify newpass>`  *If **oldpass** matches the parental lock password, and **newpass** is the same as **verify newpass**, then the parental lock password will be changed. If **pass menu** is set, then this will be displayed if everything was ok, and if **fail menu** is set, then this will be displayed if any error occurred.*
* pass menu 
* fail menu

freshgame *Starts a fresh game, bringing up a requester asking if you wish to exit the current game if you are in one.*


---

## Control

stm `<name>` `<width>` `<height>`
* nopush *this page is not pushed onto the page stack - it cannot be revisited.*
* waitanim *the page waits for the ghoul animation to finish before allowing any input. (E.g. on the animating main page)*
* fragile *Any keypress will instantly exit this screen. (E.g. On the start up animation)*
* global *disable the default scissoring to clip the contents of this page to its owning frame.*
* command `<command>` *Execute a command (normally to set up data on the page) before creating the page. (E.g. the game statistics page)*
* resize `<width>` `<height>` *Resize the current frame to a new width and height and centre on the screen. A resized menu can be locked to the position of the cursor by holding down the right mouse button anywhere inside it. (E.g. any requestor type menu).*
* default

frame `<name>` `<width>` `<height>` 
* default 
* border `<width>` `<line width>` `<line colour>` *This defines a border which fits inside the frame. Normally used to outline requesters and normally has no page attached to it.*
* page `<name>` *Defines the RmfPage (rmf) file associated with this frame.*
* cpage `<cvar>` *The parser finds the contents of the console variable and uses that as the name of the RmfPage to load.*
* cut `<name>` *Defines the name of the frame from which this frame is cut.*
* backfill `<colour>` *Defines the background colour of the frame. Mainly used for frames with no page attached.*

key `<keyname>` `<command>` *simple key -> command mapping*

ikey `<action>` `<command>` *bind command to action like +attack, instead of the key its bound to, useful if not know the key*

ckey `<keyname>` `<cvar>` `<false command>` `<true command>` *key performs 2 operations depending on cvar contain 0 or 1

comment `<text>` *just a comment*

includecvar `<cvar>` *no condition*

include `<page>` *no condition*

cinclude `<cvar>` `<page>` *condition = cvar set*

cninclude `<cvar>` `<page>` *condition = cvar clear*

exinclude `<cvar>` `<page1>` `<page2>` *condition  = cvar set : page1 , cvar clear : page2*

einclude `<cvar>` `<value>` `<page>` *condition = cvar match value*

alias `<action>` `<command>` = alias name  *create alias with command*

set `<cvar> <value>` = set %s %s *set cvar to value*

fset `<cvar> <value> <flags>` = set %s %s %s *set cvar to value with flags*

cbuf `<command>` = %s *insert text into console buffer*

config `<config>` = exec %s.cfg *run whenever the page is created or visited*

exitcfg `<config>` *run whenever a page is destroyed or left*

timeout `<value>` `<command>` *time holding on a page before executing a predetermined command*

onexit `<cvar>` `<menu>` *show another menu when exiting this menu*

---
## Layout

tint `<name>` `<colour>`

font 
* type *this can be either title, medium or small. View the different fonts by type “menu fonts” at the console.*
* tint *defines the default colour for all following text*
* atint *defines the default hilite colour for all following text.*

br *soft line break*

hbr *hard line break*

center *A layout command which forces all following areas to appear centred on the screen and applies a carriage return (I admit that this is not ideal). If you wish to centre a line with spaces in it, then place the phrase in quotes.*

normal *A layout command which places all following areas to the right of the previous area until no more will fit, and then a carriage return is applied. This is the default layout state, all areas appearing from left to right just like on a page of text.*
aka **normal** will **target the next line with free space in it**

left 
```xml
A layout command which places all the following areas from the left of the page going right. The only difference between this and <normal> is that <left> starts from the next available place on the left of the page and not the current “next area position”.
```
aka **left** will **always prioritise a new line further vertically down the frame even if off the screen, as long as that new line starts more left than the next line** 
CAVEAT: hr element cant be used cos has inbuilt left

right *A layout command which places the following areas at the right of the page working left.*

cursor `<value>`

defaults 
* noborder *All following areas are created without a border unless specifically requested.*
* border `<width>` `<line width>` `<colour>` *All following areas are created with the border defined here.*
* tiptint  `<colour>` *Defines the colour of the text in the tooltip popups.*
* tipbtint `<colour>` *Defines the colour of the background of the tooltip popups. The light edge is this colour 33% brighter, the dark edge is this colour 33% darker.*
* tipfont `<font>` *Defines the font to be used in the tooltips.*

translate `<value>`
* conv `<cvar>` `<name>` *converts a console variable name into real text. The console variable value is displayed as normal.*
* convbits `<cvar>` `<name>` `<list>` *converts each bit of the console variable’s bitfield into text.*

autoscroll `<value>`

tab `<value>` `[value]
`
strings `<sp>`

bghoul `<model>`

backdrop 
* tile *tiles the following image to fill the frame (or the whole screen if the global flag is set)*
* stretch *stretches the following image to fill the frame.*
* center *centres the following image in the frame. Does not scale at all.*
* right *puts the following image flush to the right of the screen.*
* left *puts the following image flush to the left of the screen*
* bgcolor *defines the background colour to be used.*

emote `<name>` `<text>` **linux binary only (GOLD/1.06?)**
semote `<name>` `<text>` **linux binary only (GOLD/1.06?)**
demote `<name>` **linux binary only (GOLD/1.06?)**
dmnames `<list>` **linux binary only (GOLD/1.06?)**

---

## Area

blank `<width>` `<height>`
* {PARSE_RECT}

hr

vbar
* {PARSE_RECT}

hbar
* invisible *progress bars commonly hide when the bound cvar is 0*
* {PARSE_RECT}

text `<text>`
* regular
* {PARSE_RECT}

ctext `<cvar>` *use the text stored within the cvar, invisible hides text if text is empty*
* atext 
* invisible
* {PARSE_RECT}

image `<image>` 
* abs `<x>` `<y>` *Sets the absolute position of the image avoiding all the automatic layout (not recommended)*
* alt `<image>` *Sets the alternate highlight image of this area. When the cursor moves over this area, the original image is changed to this one. This image will scale to the dimensions of the original.*
* hflip *Flips the image horizontally.*
* vflip *Flips the image vertically.*
* text `<text>` `<xoff>` `<yoff>` *adds an overlay of text at offset xoff, yoff from the top left hand corner of the image.*
* cvartext `<cvar>` `<xoff>` `<yoff>` *adds an overlay of the text contained in console variable at offset xoff, yoff from the top left of the image.*
* scale `<value>` *scales the whole image in both dimensions by this value*
* transient *this keyword instructs the image to be reuploaded every frame the contrast, brightness, gamma or intensity is changed. This is used to display gamma bars.*
* setimage `<cvar>` *the image displayed is named in the console variable string. Used for the crosshair.*
* {PARSE_RECT}

list `<list>` 
* atext `<text>` *defines text to be placed before the item.*
* match `<list>` *defines a parallel list of values that the items in the original list refer to.*
* bitmask `<bit number>` *works on the bit number of the console variable value rather than the whole console variable. Used for deathmatch flags. More than 2 items in the list could cause undefined results.*
* files `<root>` `<base>` `<ext>` *grabs the list of files from “root/base.ext” and allows the user to select through the files. Only the base part of the filename is displayed.*
* {PARSE_RECT}

slider 
* min `<value>` *the value the console variable will be set to when the button is at the far left of the slider. Defaults to 0.0f.*
* max `<value>` *the value the console variable will be set to when the button is at the far right of the slider. Defaults to 1.0f.*
* cvarmax 
* step `<value>` *modifies the step value from the default 0.1f.*
* vertical *makes the slider bar a vertical one.*
* horizontal *make the slider bar a horizontal one (default)*
* bar *overrides the default bar graphic to your custom one.*
* button *overrides the default button graphic.*
* cap *overrides the default end cap graphic.*
* {PARSE_RECT}

ticker `<text>` 
* speed *the speed at which the text scrolls. The higher the number, the faster the scroll.*
* delay *the time to wait before starting the text scrolling. This is to allow animations to finish before the text starts to scroll.*
* {PARSE_RECT}

input 
* global *any key press on the screen will go to this box. The area does not have to be highlighted to accept text.*
* hidden *all entered text comes up as *. Used to enter passwords.*
* maxchars *the input box will allow this many characters and then stop. The text will scroll to fit and clip off the left.*
* history *this input box remembers all strings typed into it. Used for chat history.*
* {PARSE_RECT}

setkey `<action>` 
* atext `<text>` *The text associated with this area. Appears to the left of the area.*
* talign {_left_ _right_ _center_} *aligns the text within the area.*
* {PARSE_RECT}

popup `<list>`
* {PARSE_RECT}

selection `<image>` `<width>` `<height>` 
* weapons *defines this selection area to take weapons.*
* items  *defines this selection area to take items.*
* ammo *displays selected ammo.*
* misc *displays selected miscellaneous console variables.*
* {PARSE_RECT}

ghoul `<model>` 
* yaw `<cvar>` *sets the models yaw to the value from **cvar***
* pitch `<cvar>` *sets the models pitch to the value from **cvar***
* roll `<cvar>` *sets the models roll to the value from **cvar***
* scale `<value>` *scales the model by value. A scale of 2.0 will double the size of the model.** 
* oneshot  *Only play the model animation once, stop at the end.*
* control  *Make this model the controlling one. Used for pages with multiple animating models.*
* time `<value>` *Sets the models animation to time **value.***
* animate *Informs the parser that this model animates.*
* rotate `<roll>` `<yaw>` `<pitch>` *sets the angular velocity of the model.*
* rotation `<roll>` `<yaw>` `<pitch>` *sets the initial rotation of the model.*
* mousex `<cvar>` `<value>` *allows the user to lock horizontal movement of the mouse while over the area. This movement is scaled by **value.***
* mousey `<cvar>` `<value>` *allows the user to lock vertical movement of the mouse while over the area. This movement is scaled by **value.***
* gbolt `<bolt1>` `<bolt2>` *bolts **bolt2** of the model to **bolt1** of the most recently defined ghoul model.*
* bgbolt `<bolt1>` `<bolt2>` *bolts **bolt2** of the model to **bolt1** of the background ghoul model.*
* end `<command>` *when the animation ends, add **command** to the exec buffer.*
* cvar2 `<cvar>` *a console variable used to store the name of the team icon.*
* {PARSE_RECT}

gpm `<cvar>` `<cvar>`
* Same as Above but *This loads and displays a sequence of gpm files (player model description files) to allow the user to pick a player model.*

filebox `<root>` `<base>` `<ext>` 
* backfill `<colour>` *defines the background fill colour of the area.*
* sort `<sort>` *Defines the sort criteria of the files to be displayed. Can be “name”, “rname”, “size”, “rsize”, “time” or “rtime”. The criteria starting with ‘r’ are from lowest to highest, rather than the other way around.*
* talign {_left_ _right_ _center_} *Defines the layout of the filenames inside the area.*
* {PARSE_RECT}

filereq `<root>` `<base>` `<ext>`
* backfill `<colour>` *defines the background fill colour of the area.*
* sort `<sort>` *Defines the sort criteria of the files to be displayed. Can be “name”, “rname”, “size”, “rsize”, “time” or “rtime”. The criteria starting with ‘r’ are from lowest to highest, rather than the other way around.*
* talign {_left_ _right_ _center_} *Defines the layout of the text inside the requestor.*
* {PARSE_RECT}

loadbox
* backfill `<colour>` *defines the background fill colour of the area.*
* sort `<sort>` *Defines the sort criteria of the files to be displayed. Can be “name”, “rname”, “size”, “rsize”, “time” or “rtime”. The criteria starting with ‘r’ are from lowest to highest, rather than the other way around.*
* spacing `<value>` *The number of pixels bordering each save game image.*
* saving *means that this area is used for saving games on this page. The autosave file is not displayed and the area will not be created if the user is not in a game.*
* {PARSE_RECT}

serverbox
* column `<index>` `<name>` *Defines the name of the column and the index of the data in the serverinfo packet. Valid indices are 0 (host name), 1 (map name), 2 (players), 3 (game type) or 4 (ping). Columns can appear in any order (or not at all) and are spaced out according to the tab settings.*
* dmnames `<list>` *Defines which game type numbers are which game type names.*
* hostname `<cvar>` *The console variable to set to the host name of the currently selected server.*
* address `<cvar>` *The console variable to set to the address (either IP or IPX) of the currently selected server.*
* {PARSE_RECT}

serverdetail
* title `<list>` *Defines the titles of the columns. Normally “Key” and “Value”. A horizontal bar is placed after the title and the parsed fields appear after that.*
* {PARSE_RECT}

players
* title `<list>` *Defines the titles of the columns. Normally “Key” and “Value”. A horizontal bar is placed after the title and the parsed fields appear after that.*
* {PARSE_RECT}

listfile `<file>`
* {PARSE_RECT}

users
* {PARSE_RECT}

chat
* {PARSE_RECT}

rooms
* {PARSE_RECT}

bot
* headings
* {PARSE_RECT}

botlist
* {PARSE_RECT}



---


## Shared Keywords to above that has {PARSE_RECT} Areas

key `<key>` `<command>` *when this area is highlighted, and key is pressed, then command is added to the exec buffer, unexpected keyname = mouse1dbl*

ckey `<key>` `<cvar>` `<false command>` `<true command>` *when this area is highlighted, and key is pressed, if cvar contains zero, then false command is added to the exec buffer otherwise true command is added.*

ikey `<action>` `<command>` *if any key bound to action is pressed while this area is highlighted, then command is added to the exec buffer.*

tint `<colour>`  *the area will have the tint of the colour. E.g. “tint blue” will filter out all red and green components*

atint `<colour>`*when this area is highlighted (ie the cursor is over the area) the area will be tinted by colour.*

btint `<colour>`*some areas require/can have a backfill colour – this is it.*

ctint `<colour>`*some areas need additional highlighting/differentiating colours – this is one of them.*

dtint `<colour>`*some areas need additional highlighting/differentiating colours – this is the other.*

noshade *some areas automatically shade different parts of their components. noshade disables this feature.*

noscale *bolt offsets are automatically scaled to the resolution. Noscale disables this feature.*

border `<width>` `<line width>` `<line colour>` *adds a border to the current area. The border is width wide, and in the centre is a line of colour line colour that is line width wide.*

width `<value>`

height `<value>`

next

prev

cvar `<cvar>` *associates a cvar with this area. Used to pass data to and from areas.*

cvari `<cvar>` *associates a cvar that can only have integer values to this area.*

inc `<value>` *specifies the step value to be used when keyboard shortcuts are used.*

mod `<value>` *specifies the modulus of the area - the value at which the console variable associated with the area will wrap around.*

tip `<text>` *specifies the tooltip for this area.*

xoff `<value>` *specifies the horizontal offset from its defined position of this area. Normally used with bolted areas.*

yoff `<value>` *as above, but vertical offset.*

tab *use a tab stop when laying out this area.*

noborder *if a border is defined in the defaults, then this turns off borders for this area.*

bolt `<bolt>` *bolts the current area to a ghoul bolt in the most recently defined ghoul model. (more bolting info)*

bbolt `<bbolt>` *bolts the current area to the ghoul bolt defined in the ghoul model used for the background (bghoul) model.*

align `<align>` *either left, center or right. Used as an additional layout tool to override the current page settings.*

iflt
ifgt
ifle
ifge
ifne
ifeq
ifset
ifclr


---

![[Pasted image 20260217062306.png]]