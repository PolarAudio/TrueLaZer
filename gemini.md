Compile error log file is at compile_error.log we check this file first to see if there are any error we have to solve before where start working on new functions


General Project Idea

A Laser Show Program that works like Resolume,
Resolume is a powerful VJ software for live video mixing and effects. It's known for its intuitive and
  customizable interface. Here's a breakdown of its UI and core features compared to other software:

  User Interface


   * Deck-based Layout: The central "deck" is a grid where you organize your media clips. This is a common
     paradigm in VJ software, but Resolume's implementation is very flexible.
   * Layers and Columns: You can stack visuals in layers and organize your performance into columns, which is
     great for structuring a show.
   * Customizable Panels: The entire interface is made of panels that you can rearrange, resize, and hide.
     This level of customization allows you to create a workspace that's perfectly tailored to your workflow,
     which is a significant advantage over more rigid interfaces.
   * Integrated Browser: A built-in browser lets you easily access and manage your media, effects, and
     sources.

  Core Features


   * Live Video Mixing: Play and mix multiple video and audio files in real-time, with control over playback
     speed and direction.
   * Extensive Effects Library: Resolume comes with over 100 built-in video effects and allows you to create
     your own using Resolume Wire, a node-based patching environment. This is a major advantage, as it offers
     limitless creative possibilities.
   * Generative Content (Sources): Besides playing video files, Resolume can generate visuals on the fly. This
      includes simple colors and text, as well as more complex generative content. This is a powerful feature
     for creating unique and dynamic visuals.
   * Audio-Visual Integration: It can sync visuals to the beat of the music through audio analysis.
   * Projection Mapping (Arena): The Arena version of Resolume has advanced tools for projection mapping,
     allowing you to map visuals onto complex surfaces. This is a high-end feature that sets it apart from
     more basic VJ software.
   * Extensive Control Options: It can be controlled with MIDI, OSC, DMX, and SMPTE timecode, allowing for
     seamless integration with other show control systems.


  In comparison to other software, Resolume's strengths lie in its combination of a user-friendly,
  customizable interface with a powerful and extensible feature set. While other VJ software might offer
  similar features, Resolume's node-based environment for creating custom effects and generators (Wire) and
  its advanced projection mapping capabilities (Arena) make it a top choice for professionals.
  
TrueLazer will be the Name of our Project and we use C++ as Core language to get the most performance possible.
we will use Dear ImGUI for the UI.(https://github.com/ocornut/imgui.git)
  
Our Goal is to code a Program that works like Resolume But for Laser/ILDA usage.
We use Showbridge as our DAC so we have to learn from the Truewave SDK folder how our Software send the ILDA data to the showbridge DAC/Interface.
MIDI and DMX/Artnet integration is very important and should work in the end by activate the midi or DMX learn mode and learn each button or slider to the midi/DMX command it recieve. 
We Use mostly Artnet/shownet for our DMX mapping so we need everything to achieve that functions.

A list of things we change inside of the UI:
	
  User Interface

   * Deck-based Layout: The central "deck" is a grid where you organize your ilda clips.
   * Layers and Columns: You can stack clips in layers and organize your clips into columns.
   * Customizable Panels: The entire interface is made of panels that you can rearrange, resize, and hide.
     This level of customization allows you to create a workspace that's perfectly tailored to your workflow,
     which is a significant advantage over more rigid interfaces.
   * Integrated Browser: A built-in browser lets you easily access and manage your media, effects, and
     sources.

  Core Features

   * Live ILDA Mixing: Play and mix multiple ILDA and audio files in real-time, with control over playback
     speed and direction.
   * Extensive Effects Library: built-in effects (like Rotation,Scale,Transform etc.) allows us to create or manipulate Sources by draging them on a ILDA CLIP
	 If Possible for us we can create a Node Based effect window that opens if we want to customize effects. Like the resolume Wire Example(This is a visual, node-based programming environment that comes with Resolume. It allows
     you to create your own effects, sources, and mixers without writing traditional code. You connect nodes
     together to build your logic.)
	 We create some premade effects but leave the option to create own effects and save them.
   * Generative Content (Sources): Besides playing ILDA files, We want to generate ILDA on the fly. This
      includes simple colors, dots, lines, circles and text, as well as more complex generative content.
   * Audio-Visual Integration: We want to sync Clips to the beat of the music through audio analysis.
   * Projection Mapping : We need to edit Outputs for each laser projector like ip-adress, size, safe-zones if possible some option to adjust warping would be also great.
   * Extensive Control Options: It can be controlled with MIDI, OSC, DMX, allowing for
     seamless integration with other show control systems.
	 
Our ShowBridge protocol follows this pattern:
	Broadcast Message to 255.255.255.255:8089 with
    Command: 6 bytes = Target IP (169.254.25.104) + Flags (163, 31).
	
	Answer from DAC to "Target IP":8099
    Response: 16 bytes = Vendor ID (22,26) + Type (1) + Channel (1/2) + Device ID (630380) + Value (63) + Checksum (0x2237/0x2238).

The labels 7 and 8 are extracted from the checksum’s last nibble, matching the device’s channel identifiers.