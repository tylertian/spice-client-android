#**Introductions to port spice-client onto android**

# Introduction #
> This port is based on the spice-gtk-0.5 with which as the data layer along with the  UI layer rewritten in Java. It's released under LGPL, welcome to use and improve!

# How to build #
(cmdline way,Eclipse way may diff)
1.prepare and port the prerequisites: glib-2.28.1,pixman-0.20.0,jpeg-6b and openssl-1.0.0 onto android(see the section "Prerequisites" below)
2.download the source here and create the android project.
3.
1)$android update project
> to build libspicec.so,in commandline,use
2)$ndk-build
> then build the SDK project
3)$ant debug

> ~~If your want to use the built androidspice to connect your VM,you should also force JPEG compression in Spice-server.(see section "Details")~~

# Prerequisites #
> Too many,so I build them statically into libspicec.so(you may check this in Android.mk files in project)
> The introductions of porting them along with all the tools,patches,and steps,can be found in my blogs about spice-snappy porting:
http://blog.csdn.net/rozenix
> or your may check my mails (shohyanglim@gmail.com)in spice-dev-list,if none,mail to me,please!

# Details #
> The damned greatest obstacle I've faced in the porting lies in the structure of Android:It has no(at least for version<2.3 ) exposed audio/image output and input API for C(only Java!)! So I have to transport all the fixed data got from spice-server to Java layer by adding two new threads to handle the I/O communication with Java UI via two UNIX-sockets,that's the leg-drawing of speed. ~~Besides, quic.c in client is buggy of SIGBUS or SIGSEGV on android(no time to rewrite it now),~~I have no better way but to force use of JPEG compression in server and send jpeg data directly to Java UI for output, it's queer and should be condemned('cause Spice's value is in the Image processing ability),
So now It just WORKS, but hard to escape the label of EXPERIMENTAL.

# Update #
> [https://code.google.com/p/spice-client-android/issues/detail?id=](https://code.google.com/p/spice-client-android/issues/detail?id=) fixed and the USABLE anroidSpice-0.1.4 is released both for source and apk files.
> Now the server should have no modification at all with all client bugs fixed for  androidSpice-0.1.6. But the speed will not level the server-jpeg way.
# Use PS #
> ~~To link with your VM for androidSpice-0.1.4,modify the spice server to force the use of JPEG compression and rebuild/reinstall it.(you may check the red\_worker.c in the Download page here)
> Then start VM without "-vga qxl" and with "-usbdevice tablet" in qemu cmds.~~

> For androidSpice-0.1.6, only "-usbdevice tablet" is left necessary.