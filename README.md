short:    AsyncIO library for fast IO. V39.3
uploader: cmh@lls.se (Magnus Holmgren)
type:     dev/c

This link library (optionally shared library) provides DOS-like functions
for using double buffered asyncronous IO on files. This provides very good
performance on DMA devices.

The code was originally written by Martin Taillefer. A few bug fixes and
updates were made by Magnus Holmgren, with inspiration from Olaf 'Olsen'
Barthel.

News in version 39.2a

	AsyncIO.guide was missing from the archive, making it difficult for
	new users of AsyncIO. Also added a few icons. These are the only
	changes in this	release.

Re-release of version 39.2

	The version string in the last archive wasn't correct. This is the
	only thing changed in this release.

News in version 39.2:

	Fixed bugs in ReadLineAsync(), ReadCharAsync() and WriteCharAsync().

	SeekAsync() is now more DOS-compatible, in that it allows seeks to
	succeed after seeks past EOF. Thus, it is now possible to seek back
	to a valid position in the file and continue reading there.

