## Description

Slug is an (incomplete) scriptable BitTorrent daemon for Unix-like systems.

I decided to create Slug because I'm fed up with the lackluster automation tools offered by other BitTorrent clients. I wanted a client that exposed a simple API combined with pattern matching which would allow an individual to write powerful pre- and post-hooks to be executed when a torrent that matches a pattern is started or finished.

Example usages/features:

* Downloading every torrent from an RSS feed and then moving it to another folder automatically.
* Moving a torrent to a specific folder based on a pattern, like moving torrents that match the regex "*[Ll]ost*" to "/tv/lost".
* Automatically unraring a torrent and moving the unrared file(s) to a folder.
* Contacting a metadata server and renaming a torrent based on the metadata recieved from the server.
* Scheduling specific hours during which Slug should run.
* Stopping a torrent after a certain amount of time or once it has seeded to a certain ratio.

All of these things will be possible with Slug and, most importantly, they won't stop the torrents from seeding -- even after being renamed, etc. This will be accomplished by embedding Lua for use as a scripting language inside of Slug and allowing users to write "rule files" to complete tasks. Slug's configuration will be exposed via a default rule file.

Slug, by itself, will not have a GUI but -- once I finish Slug -- I intend to write a user-friendly WebUI that can optionally be used alongside Slug. This will expose actions such as pausing and removing torrents and make torrent management a breeze, even when dealing with thousands of torrents. Ideally, you will be able to think of Slug's webUI as an even better version of Rutorrent.

## How can I contribute?

Contributing is easy. Just fork the project, make your changes, and then submit a pull request. 

Some stuff that needs done:

* Clean-up the codebase to match [OpenBSD's style guidelines](http://www.openbsd.org/cgi-bin/man.cgi?query=style&apropos=0&sektion=0&manpath=OpenBSD+Current&arch=i386&format=html). All of the linked lists need be reimplemented to use the macros from <queue.h>.
* Lots of error checking has been ignored and needs added.
* Read through the code and find areas that ought to be explained by a comment but aren't. Comment them or tell me to comment them.
* Implement support for multi-file torrents. For more information, you can look at how [unworkable](http://code.google.com/p/unworkable/source/browse/#svn%2Ftrunk) does it.
* Improve out queueing strategy. If you know C++, you could go dig through rtorrent's code, figure out their download strategy, and explain it to me so I can implement it.
* Implement a variable length queue that's based on some network speed heuristic.
* Flesh out announce support. 