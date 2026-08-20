# Catstructor
A DLL mod for Mewgenics that adds an in-game editor debug tool that can be used to create and export your own custom cats!

<img width="1920" height="1080" alt="686060_222" src="https://github.com/user-attachments/assets/e7c36f36-4088-4a8f-8f2d-64ffd093f205" />

## Using the Editor

### Accessing the Editor

Catstructor requires **Developer Mode** to be enabled to access the editor, as it uses the game's special debugging tools screen.

The recommended method is to enable the **Enable Dev Mode** option in **Mewtator** (https://www.nexusmods.com/mewgenics/mods/1).

Alternatively, add the following command to the game's Steam launch options:

```text
-dev_mode true
```

After launching the game with Developer Mode enabled, click the new **Cat Editor** button near the bottom-left corner of the screen.

### Editor Controls Overview

Use the sliders, `<` and `>` buttons, dropdowns, and the clickable **ID:** button input fields to change the different parts of your cat!

### Editor Buttons

* **SAVE** - Saves your cat as a `.catstruct` file in:

  ```text
  Catstructor\saved_cats
  ```

  Loading a saved cat and clicking **SAVE** again will update that file

* **NEW CAT** - Creates a new randomized, unsaved cat

* **RANDOMIZE** - Randomizes the current cat's appearance

* **COPY DATA** - Copies the cat's complete `.gon` data entry to your clipboard

* **SAVE PNG** - Saves a PNG image of your cat in:

  ```text
  Catstructor\cat_images
  ```

* **EXIT GAME** - Immediately closes the game

### Voice Controls

Choose a voice from the **voice** dropdown, adjust the **pitch** slider, and click **MEOW** to preview it.

### Loading/Saving Cats

Use the **custom_cats.gon preset** dropdown to load existing cats from the game's `custom_cats.gon` file

Use the **saved cats** dropdown to load or delete `.catstruct` files from the `saved_cats` folder.

### Custom Stray Framework

Catstructor is recommended for use with the [Custom Stray Framework](https://www.nexusmods.com/mewgenics/mods/434), as it makes it easier to add and manage custom stray cats in your mods.

### Custom Cat Part/Texture Support

Catstructor is compatible with [MewCatPartFramework](https://www.nexusmods.com/mewgenics/mods/489). Please read [this tutorial](https://github.com/Pseudonym-Tim/mew-cat-part-framework/#making-a-custom-cat-part-mod) explaining how you can add your own custom cat parts and textures. 

You can have your cat use a custom modded part or texture by clicking the "**ID:**" field button next to **palette**, then typing in it's custom ID name, such as:

```text
@myPartOrTexture
```

You should then see that your cat updates to reflect the changes.

Custom part/texture IDs are preserved when saving your cat or copying its `.gon` data.

### Custom Palette Support

Catstructor is compatible with [MewPaletteExtender](https://www.nexusmods.com/mewgenics/mods/369). Please read [this tutorial](https://github.com/Pseudonym-Tim/mew-palette-extender#making-a-custom-palette-mod) explaining how you can add your own custom palettes. 

You can have your cat use a custom modded palette by clicking the "**ID:**" field button next to **palette**, then typing in the palette's custom ID name, such as:

```text
@myPalette
```

You should then see that your cat updates to reflect the colors of your palette.

Custom palette IDs are preserved when saving your cat or copying its `.gon` data.

## TODO/Future Support

* ~A cat part and texture framework that will interface with this editor, similar in functionality to MewPaletteExtender. This will allow modders to easily add brand new mod-compatible things such as: cat body pieces, cat textures, and item visuals.~ **EDIT: A cat part and texture framework has been released, but at the time of writing, it only supports cat-specific stuff rather additional than things such as items.**

* The ability to add and view various items/equipment on your cats

* Mutation info for your cat's currently selected parts/textures

* Toggling visibility of individual cat parts
