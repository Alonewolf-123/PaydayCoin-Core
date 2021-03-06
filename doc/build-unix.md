UNIX BUILD NOTES
+AD0APQA9AD0APQA9AD0APQA9AD0APQA9AD0APQA9AD0APQA9AD0APQ-
Some notes on how to build PaydayCoin Core in Unix.

(For BSD specific instructions, see +AGA-build-*bsd.md+AGA- in this directory.)

Note
---------------------
Always use absolute paths to configure and compile PaydayCoin Core and the dependencies.
For example, when specifying the path of the dependency:

	../dist/configure --enable-cxx --disable-shared --with-pic --prefix+AD0AJA-BDB_PREFIX

Here BDB_PREFIX must be an absolute path - it is defined using +ACQ-(pwd) which ensures
the usage of the absolute path.

To Build
---------------------

+AGAAYABg-bash
./autogen.sh
./configure
make
make install # optional
+AGAAYABg-

This will build paydaycoin-qt as well, if the dependencies are met.

Dependencies
---------------------

These dependencies are required:

 Library     +AHw- Purpose          +AHw- Description
 ------------+AHw-------------------+AHw-----------------------
 libssl      +AHw- Crypto           +AHw- Random Number Generation, Elliptic Curve Cryptography
 libboost    +AHw- Utility          +AHw- Library for threading, data structures, etc
 libevent    +AHw- Networking       +AHw- OS independent asynchronous networking

Optional dependencies:

 Library     +AHw- Purpose          +AHw- Description
 ------------+AHw-------------------+AHw-----------------------
 miniupnpc   +AHw- UPnP Support     +AHw- Firewall-jumping support
 libdb4.8    +AHw- Berkeley DB      +AHw- Wallet storage (only needed when wallet enabled)
 qt          +AHw- GUI              +AHw- GUI toolkit (only needed when GUI enabled)
 protobuf    +AHw- Payments in GUI  +AHw- Data interchange format used for payment protocol (only needed when GUI enabled)
 libqrencode +AHw- QR codes in GUI  +AHw- Optional for generating QR codes (only needed when GUI enabled)
 univalue    +AHw- Utility          +AHw- JSON parsing and encoding (bundled version will be used unless --with-system-univalue passed to configure)
 libzmq3     +AHw- ZMQ notification +AHw- Optional, allows generating ZMQ notifications (requires ZMQ version +AD4APQ- 4.0.0)

For the versions used, see +AFs-dependencies.md+AF0-(dependencies.md)

Memory Requirements
--------------------

C compilers are memory-hungry. It is recommended to have at least 1.5 GB of
memory available when compiling PaydayCoin Core. On systems with less, gcc can be
tuned to conserve memory with additional CXXFLAGS:


    ./configure CXXFLAGS+AD0AIg---param ggc-min-expand=1 --param ggc-min-heapsize=32768+ACI-


+ACMAIw- Linux Distribution Specific Instructions

+ACMAIwAj- Ubuntu +ACY- Debian

+ACMAIwAjACM- Dependency Build Instructions

Build requirements:

    sudo apt-get install build-essential libtool autotools-dev automake pkg-config bsdmainutils python3

Now, you can either build from self-compiled +AFs-depends+AF0-(/depends/README.md) or install the required dependencies:

    sudo apt-get install libssl-dev libevent-dev libboost-system-dev libboost-filesystem-dev libboost-chrono-dev libboost-test-dev libboost-thread-dev

BerkeleyDB is required for the wallet.

Ubuntu and Debian have their own libdb-dev and libdbdev packages, but these will install
BerkeleyDB 5.1 or later. This will break binary wallet compatibility with the distributed executables, which
are based on BerkeleyDB 4.8. If you do not care about wallet compatibility,
pass +AGA---with-incompatible-bdb+AGA- to configure.

Otherwise, you can build from self-compiled +AGA-depends+AGA- (see above).

To build PaydayCoin Core without wallet, see +AFsAKg-Disable-wallet mode+ACoAXQ-(/doc/build-unix.md#disable-wallet-mode)


Optional (see --with-miniupnpc and --enable-upnp-default):

    sudo apt-get install libminiupnpc-dev

ZMQ dependencies (provides ZMQ API):

    sudo apt-get install libzmq3-dev

GUI dependencies:

If you want to build paydaycoin-qt, make sure that the required packages for Qt development
are installed. Qt 5 is necessary to build the GUI.
To build without GUI pass +AGA---without-gui+AGA-.

To build with Qt 5 you need the following:

    sudo apt-get install libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev qttools5-dev-tools libprotobuf-dev protobuf-compiler

libqrencode (optional) can be installed with:

    sudo apt-get install libqrencode-dev

Once these are installed, they will be found by configure and a paydaycoin-qt executable will be
built by default.


+ACMAIwAj- Fedora

+ACMAIwAjACM- Dependency Build Instructions

Build requirements:

    sudo dnf install gcc-c libtool make autoconf automake openssl-devel libevent-devel boost-devel libdb4-devel libdb4-cxx-devel python3

Optional:

    sudo dnf install miniupnpc-devel

To build with Qt 5 you need the following:

    sudo dnf install qt5-qttools-devel qt5-qtbase-devel protobuf-devel

libqrencode (optional) can be installed with:

    sudo dnf install qrencode-devel

Notes
-----
The release is built with GCC and then +ACI-strip paydaycoind+ACI- to strip the debug
symbols, which reduces the executable size by about 90+ACU-.


miniupnpc
---------

+AFs-miniupnpc+AF0-(http://miniupnp.free.fr/) may be used for UPnP port mapping.  It can be downloaded from +AFs-here+AF0-(
http://miniupnp.tuxfamily.org/files/).  UPnP support is compiled in and
turned off by default.  See the configure options for upnp behavior desired:

	--without-miniupnpc      No UPnP support miniupnp not required
	--disable-upnp-default   (the default) UPnP support turned off by default at runtime
	--enable-upnp-default    UPnP support turned on by default at runtime


Berkeley DB
-----------
It is recommended to use Berkeley DB 4.8. If you have to build it yourself,
you can use +AFs-the installation script included in contrib/+AF0-(/contrib/install_db4.sh)
like so:

+AGAAYABg-shell
./contrib/install_db4.sh +AGA-pwd+AGA-
+AGAAYABg-

from the root of the repository.

**Note**: You only need Berkeley DB if the wallet is enabled (see +AFsAKg-Disable-wallet mode+ACoAXQ-(/doc/build-unix.md#disable-wallet-mode)).

Boost
-----
If you need to build Boost yourself:

	sudo su
	./bootstrap.sh
	./bjam install


Security
--------
To help make your PaydayCoin Core installation more secure by making certain attacks impossible to
exploit even if a vulnerability is found, binaries are hardened by default.
This can be disabled with:

Hardening Flags:

	./configure --enable-hardening
	./configure --disable-hardening


Hardening enables the following features:
* _Position Independent Executable_: Build position independent code to take advantage of Address Space Layout Randomization
    offered by some kernels. Attackers who can cause execution of code at an arbitrary memory
    location are thwarted if they don't know where anything useful is located.
    The stack and heap are randomly located by default, but this allows the code section to be
    randomly located as well.

    On an AMD64 processor where a library was not compiled with -fPIC, this will cause an error
    such as: +ACI-relocation R_X86_64_32 against +AGA-......' can not be used when making a shared object+ADsAIg-

    To test that you have built PIE executable, install scanelf, part of paxutils, and use:

    	scanelf -e ./paydaycoin

    The output should contain:

     TYPE
    ET_DYN

* _Non-executable Stack_: If the stack is executable then trivial stack-based buffer overflow exploits are possible if
    vulnerable buffers are found. By default, PaydayCoin Core should be built with a non-executable stack,
    but if one of the libraries it uses asks for an executable stack or someone makes a mistake
    and uses a compiler extension which requires an executable stack, it will silently build an
    executable without the non-executable stack protection.

    To verify that the stack is non-executable after compiling use:
    +AGA-scanelf -e ./paydaycoin+AGA-

    The output should contain:
	STK/REL/PTL
	RW- R-- RW-

    The STK RW- means that the stack is readable and writeable but not executable.

Disable-wallet mode
--------------------
When the intention is to run only a P2P node without a wallet, PaydayCoin Core may be compiled in
disable-wallet mode with:

    ./configure --disable-wallet

In this case there is no dependency on Berkeley DB 4.8.

Mining is also possible in disable-wallet mode using the +AGA-getblocktemplate+AGA- RPC call.

Additional Configure Flags
--------------------------
A list of additional configure flags can be displayed with:

    ./configure --help


Setup and Build Example: Arch Linux
-----------------------------------
This example lists the steps necessary to setup and build a command line only, non-wallet distribution of the latest changes on Arch Linux:

    pacman -S git base-devel boost libevent python
    git clone https://github.com/paydaycoin/paydaycoin.git
    cd paydaycoin/
    ./autogen.sh
    ./configure --disable-wallet --without-gui --without-miniupnpc
    make check

Note:
Enabling wallet support requires either compiling against a Berkeley DB newer than 4.8 (package +AGA-db+AGA-) using +AGA---with-incompatible-bdb+AGA-,
or building and depending on a local version of Berkeley DB 4.8. The readily available Arch Linux packages are currently built using
+AGA---with-incompatible-bdb+AGA- according to the +AFs-PKGBUILD+AF0-(https://projects.archlinux.org/svntogit/community.git/tree/paydaycoin/trunk/PKGBUILD).
As mentioned above, when maintaining portability of the wallet between the standard PaydayCoin Core distributions and independently built
node software is desired, Berkeley DB 4.8 must be used.


ARM Cross-compilation
-------------------
These steps can be performed on, for example, an Ubuntu VM. The depends system
will also work on other Linux distributions, however the commands for
installing the toolchain will be different.

Make sure you install the build requirements mentioned above.
Then, install the toolchain and curl:

    sudo apt-get install garm-linux-gnueabihf curl

To build executables for ARM:

    cd depends
    make HOST=arm-linux-gnueabihf NO_QT=1
    cd ..
    ./autogen.sh
    ./configure --prefix+AD0AJA-PWD/depends/arm-linux-gnueabihf --enable-glibc-back-compat --enable-reduce-exports LDFLAGS=-static-libstdc
    make


For further documentation on the depends system see +AFs-README.md+AF0-(../depends/README.md) in the depends directory.
