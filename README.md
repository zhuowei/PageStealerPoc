Failed attempt to exploit Xen's PageStealer bug (XSA-148)

It can map the l1 (smallest) pagetable as a huge writable page, and it can read what looks like the real page 0 from dom0, but I'm not sure whether I've actually managed to read the real memory or how to read other areas of memory. (Reading 0x100000 gives 0xc2s everywhere, which is of course wrong)

Hope someone else smarter could figure this out.
If you want to make this work, it's under GPLv2.
