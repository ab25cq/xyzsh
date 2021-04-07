
# "xyzsh" shell script language

version 1.5.8

# What is this software?

An interactive shell script language on OSX, cygwin, Linux, FreeBSD. This features simple object oriented scripting with garbage collection, text processing like perl or ruby and containes manual for all inner commands with their samples. Offcourse you can type commands with completion which is based on xyzsh grammer like IDE and is defined by itself.

    > ls -F   docs/ main.c samples/ sub.c
        
    > ls | each ( | chomp | -d && | print )
    docs
    samples

    > ls -F | grep -v /
    main.c sub.c

    > ls | each ( ! | chomp | -d && | print )
    main.c sub.c

    > ls | each ( | printf "-+- %s -+-"\n ) | join " "
    -+- docs -+- -+- main -+- -+- samples -+- -+- sub.c -+-

    > ls | each ( | scan . | join " " )
    d o c s
    m a i n . c
    s a m p l e s
    s u b . c

    > cat data
    Gorou Yamanaka
    Rei Tanaka
    Yumiko Shiratori

    > cat data | each ( | =~ Yamanaka && | print )
    Gorou Yamanaka

    > cat data | lines 0 1
    Gorou Yamanaka
    Rei Tanaka
        
    > cat data | each ( | split | lines 1 0 | join )
    Yamanaka Gorou
    Tanaka Rei
    Shiratori Yumiko
        
    > vim student.xyzsh
    class Student ( 
        | var first_name second_name age country
        
        def show (
            var first_name second_name age country | printf "name: %s %s?nage: %s?ncountry: %s?n"
        )
    )
        
    object student
    student::run ( print "Yamanaka Gorou 23 Japan" | split | Student )
        
    > load student.xyzsh
    > student::show
    name: Yamanaka Gorou
    age: 23
    country: Japan

    > ls | scan . | while(| eval "|${I=1} print |>") (|> join ""; ++ I )
    A
    UT
    HOR
    SCHA
    NGELO
    GLICEN
    SEMakef
    ileMakef
    ile.inREA
    DMEREADME.
    jaUSAGEUSAG
    E.jacompleti
    on.xyzshconfi
    g.hconfig.h.in
    config.logconfi
    g.statusconfigur
    econfigure.inhelp
    .xyzshinstall.shli
    bxyzsh.1.7.0.dylibl
    ibxyzsh.1.dyliblibxy
    zsh.dylibmanread_hist
    ory.xyzshsrcxyzshxyzsh
    .dSYMxyzsh.xyzsh

    > ls | lines 0 1 | scan . | each ( | chomp | x 5 | pomch )
    AAAAA
    UUUUU
    TTTTT
    HHHHH
    OOOOO
    RRRRR
    SSSSS
    CCCCC
    HHHHH
    AAAAA
    NNNNN
    GGGGG
    EEEEE
    LLLLL
    OOOOO
    GGGGG

Compiled and tested xyzsh below Operating systems
    OSX 10.8.0
    FreeBSD 7.1-RELEASE-p16
    CentOS 5.8
    Oracle Solaris 11 Express

# How to compile
Before compiling xyzsh, you need to resolve dependencies below. You need development packages.(ex. ncurses-devel or libncurse-dev)

    gcc 
    GNU make
    libc
    libm 
    ncurses(w)
    editline
    oniguruma (which is a regex library)
    iconv

Have you installed above libraries? You can type below commands to compile xyzsh.

    ./configure --with-optimize
    make
    sudo make install

or

    ./configure --with-optimize
    make
    su
    make install
    exit

Default prefix is /use/local/bin, so you need a root comission for make install.

If you want to change an installed directory, type below

    ./configure --prefix=??? --with-optimize
    make
    sudo make install

or

    ./configure --with-optimize
    make
    sudo make DESTDIR=??? install

When you choise normal install, installed xyzsh to /usr/local/bin and setting files to /usr/local/etc/xyzsh.

configure options are below.

    --prefix --> indicate installed directory. If you use --prefix=$HOME, xyzsh will be installed $HOME/bin, $HOME/etc. As default, prefix is /usr/local.
    --with-optimize --> compiled with optimize. Fast binary will be made.
    --with-onig-dir --> indicate oniguruma installed directory.
    --with-editline-dir --> indicate editline installed directory.
    --with-debug --> give -g option to CFLAGS and add a checking memory leak system to xyzsh.
    --with-gprof-debug --> give -pg option to CFLAGS
    --with-static --> make xyzsh without dynamic linked libraries.

xyzsh makes library, so add /usr/local/lib(or your installed directory)  to /etc/ld.so.conf and type below to refresh dynamic loarding searched path cache in the case of linux
    
    sudo ldconfig
    
or

    su
    ldconfig
    exit

If you don't have root comission, add the path to LD_LIBRARY_PATH environment variable.

with OS X

You don't have to be setting on ldconfig, but you need to set DYLD_LIBRARY_PATH like below

export DYLD_LIBRARY_PATH=/usr/local/lib

with cygwin

You need to compile oniguruma(version 5.9.4) and install it instead of cygwin's libonig-devel

You can download to it from

http://www.geocities.jp/kosako3/oniguruma/index_ja.html

Encoding and Line field

xyzsh script source file must be written with UTF-8 encode and LF Linefield. But, xyzsh can treat UTF8, EUCJP, and SJIS encodings and can treat LF, CR, LFCR line fields. (EUCJP and SJIS are for Japanese)
    You should run xyzsh as interactive shell on UTF-8 terminal.

# Usage
Type

    > xyzsh

to use as interactive shell

or

    > xyzsh -c "command"

to run the command

or

    > xyzsh (script file name)

to run the script file

# LINCENSE
    MIT LISENCE 

# USAGE

# 0.0 Introduction

Welcome to xyzsh world!

xyzsh is an interactive shell and a text processing tool.

It contains a text processing inner commands like Perl or Ruby, and can be used as a simple objective oriented script language.

Ofcourse, on interactive shell, it helps you to type with completion.

This program linces is MIT Lisence.

You can get this on the internet belows

# 1.0 statment

(::)object::object::,...,::object (argument|-option|block)*

You can use non alphabet characters for object name.

ex)

    > print 1 | * 2
    2

# 1.1 quote

If you want to use xyzsh special characters for command name or arguments, you should quote before the special characters.

How to quote special characters is

1. use single quotes  (ex.) ';*?[]()'
2. use double quotes (ex.) ";*?[]()"
3. use a quote before a special character (ex.) \;
4. use %q() or %Q()

An quote is no effect in another quote.

    > print "'string'"  # --> 'string' outputs

You can use a control character putting a special alphabet after quote

\n --> line field
\r --> carrige return
\a --> bel

    > print "Hello World"\n   # --> outputss "Hello World" and a linefield

Pay attension to be unable to use control characters in single quotes or double quoutes

    > print "Hello World\n"    # --> output "Hello World" and not linefield but "\" and "n" 

USAGE of %q(), %Q() is below

    > sys::ruby -e %q<print "'Hello World'\n">
    'Hello World'

    > sys::ruby -e %q!print "'Hello World'\n"!
    'Hello World'

    > print %q(Hello World\n)
    Hello World\n

    > print %Q/Hello World\n/
    Hello World

In the case %Q, expand escape sequences and variables.

# 1.2 pipe and context pipe

Commands can be connected using a pipe. Using xyzsh, a command input is previous command output and a command output is next command input.

    > ls | sub -global . X

    Example for context pipe

    > ls | ( | less; | sub -global . X )

() is subshell which can treat a block as one command. | less is that less gets ls output using context pipe.
Context pipe is a pipe which is changed its content responding to a command.
In this case, a context pipe gets output of ls.

# 1.3 statment sparator and return code

;, \n normal separator
|| If previous statment return code is false, run the next command
&& If previous statment return code is true, run the next command
& If last command is external command, run the external command as background

Putting ! on head of statment, reverse the return code

    > ! true
    return code is 1

    > ! false

# 1.4 Options

An argument which begin from - is a option. For external programs, it is treated a normal argument like another shell.

If you don't want to treat the string as option, you can quote head of the string.

    > sub -global . X

    -global is given as option to sub internal command

    > print "-aaa" | less

    -aaa is not option

But

    >  echo -aaaa | less

The case of an external command like echo, -aaaa string is given as no option to echo.

# 1.5 Basic commands

"if" inner command acts as conditional jump.

if (one condition) (if one condition is true, run this block) (second conditon) (if second condition is true, run this block),..., (if all block is not runned, run this block)

You can add "else" or "elif" words to command line freely.

    > var a | if(|= main\n) (print true\n) else (print false\n)

If output of a is main\n, print true. If output of a is not main\n, print false.

There are below conditional inner commands

    -n if input exists, return code is 0 (which represents true)
    -z if input doesn't exist, return code is 0
    -b if the file exists and it's a block special file, return code is 0
    -c if the file exists and it's a chalacter special file, return code is 0
    -d if the file exists and it's a directory, return code is 0
    -f if the file exists and it's a regular file, return code is 0
    -h 
    -L if the file exists and it's a sibolic link, return code is 0
    -p if the file exists and it's a named pipe, return code is 0
    -t
    -S if the file exists and it's a socket, return code is 0
    -g if the file exists and it's setted GID, return code is 0
    -k if the file exists and it's setted sticky bit, return code is 0
    -u if the file exists and it's setted SUID, return code is 0
    -r if the file exists and it's readable, return code is 0
    -w if the file exists and it's writable, return code is 0
    -x if the file exists and it's excutable, return code is 0
    -O if the file exists and it's owned by the user, return code is 0
    -G if the file exists and it's owned by the group, return code is 0
    -e if the file exists , return code is 0
    -s if the file exists and the size is begger, return code is 0
    =  the input equals to the argument, return code is 0
    !=  the input doesn't equals to the argument, return code is 0
    -slt the input is smaller than argument as string, return code is 0
    -sgt the input is bigger than argument as string, return code is 0
    -sle the input is smaller than argument or equals to the argument as string, return code is 0
    -sge the input is bigger than argument or equals to the argument as string, return code is 0
    -eq If the input equals to the argument as numeric
    -nq If the input doesn't equal to the argument as numeric
    -lt If the input is smaller than the argument as numeric
    -le If the input is smaller than the argumrnt or equals to the argument as numeric
    -gt If the input is bigger than the argument as numeric
    -ge If the input is bigger than the argumrnt or equals to the argument as numeric
    -nt If the input is newer than the argumrnt
    -ot If the input is older than the argumrnt
    -ef If inode of the input file equals to inode of the argument file
    =~ If the input matches the argument regex(setted local variable PREMATCH, MATCH, POSTMATCH, 1,2,3,4,5,6,7,8,9,0)

You can connect to another command with xyzsh

    > var a | if(|= main\n) (print true\n) else (print false\n) | scan . | less

"while" inner command is loop

    > print 0 | var a; while(var a | -le 5) ( print $a; ++ a)
    012345
    
You can connect to another command with xyzsh

    > print 0 | var a; while(var a | -le 5) (print $a; ++ a ) | pomch

If the user type C-c, xyzsh will occur signal interuput error

# 1.6 Variant

There are three variants on xyzsh.

1. string or integer

Var contains one line string. You can use it for integer./

    > ls | var a b c   
    Three lines of ls output is contained in a,b,c variant.

If you want to output the variant

    > var a b c
    1st line of ls output
    2nd line of ls output
    3rd line of ls output

The output contains linefields

If you want to reffer the variant

    > print "$a $b $c" \n
    (1st line of ls output) (2nd line of ls output) (3rd line of ls output)

2. dynamic array

If you want to contain all output

    > ls | ary a

ary a will contain all output of ls. One element contains one line.

If you want to output the all of elements

    > ary a

If you want to output the one element

    > ary -index 1 a

If you want to reffer thr all elements

    > print $a

The above will give all elements for "print" inner command.

     > print $a[0]

The above will give the head of elements for "print" inner command.

3. hash table

Hash table contains all lines, and xyzsh assumes that the line of odd numver is the key and the line of even number is the item.

    > print "key1 item1 key2 item2" | split | hash a

The above is evaled as that the keys of hash a is "key1", "key2" and the items of hash a is "item1", "item2".

If you want to output all elements

    > hash a
    key2
    item2
    key1
    item1

The order of output is not the same for the order of inout.

If you want to output one element

    > hash -key key1 a
    item1

If you want to reffer the hash item

    > print $a[key1]

The above will give the item of key1 for "print" inner command.

Also there is the variant kind.  Local variants or attributs.

The order of xyzsh reffering variant is local variants -> the attributs of current object -> the attributs of the parent -> ,,,

Next paragraph stated about current object.

If you want to make a local variable

    > ls | var -local a b c

Use -local option

xyzsh initialize the local variants on the next three times

1. loading script 2. calling method 3. calling class

Also every time on running cmdline.

    > ls | var -local a b c
    > var a b c
    not found variable

    > ls | var -local a b c ; var a b c

is outputted

The variable which containes option is quoted on normal way.
If you want to reffer option by variable, you use $-{var}.

    > print "1234512345" | index -count 2 1
    5
    > print "-count" | var a
    > print "1234512345" | index $a 2 1
    invalid command using
    > print "1234512345" | index $-a 2 1
    5

If you want to reffer and initialize a variable at the same time, use below.

    > print ${A=1}\n; ++ A; print $A\n
    1
    2

$A is a local variable.

This may be useful on loop like below.

    > while(print ${I=0} | -lt 5) ( print HELLO\n; ++ I )
    HELLO
    HELLO
    HELLO
    HELLO
    HELLO

# 1.7 Object Oriented Programing

Object Oriented Programing of xyzsh likes a file system.
A object like a directory and is added to other objects(var, hash, ary, and object) 

    > object a       # add object a to current object
    > a::run ( ls | ary A )      # add ary A to object a
    > a::A
    output of ls

a::run is runned on "a" object as current object.
ary A is added to "a" object.
object "a" is added to root object because initial current object is root object.

    > pwo

shows a current object

    > self

lists all objects at current object

If you runned "self" at root obhect, you can look at defined inner command objects at root object.
The order of xyzsh object reffering, is local objects -> current objects -> parent objects,...

Therefore you can access inner commands in child object.

Also all objects have

"run" method
"show" method
"self" object (refference of its self)
"root" (refference of root object)
parent (refference of parent object)

so

    > co parent

makes current object changed as parent object

If you want to remove a object

    > sweep (object name)

run above.

# 1.8 Function (method)

Function is added with "def" inner command to current object.

    > def fun ( print fun \n)
    > fun
    fun
    > self | egrep ^fun:
    fun: function

There are function argumrnts in ARGV array.

    > def fun ( print $ARGV )
    > fun aaa bbb ccc
    aaa bbb ccc

There are function options in OPTIONS hash table.

    > def fun ( hash OPTIONS )
    > fun -abc -def
    -abc
    -abc
    -def
    -def

You can use -option-with-argument for "def" inner command.

    > def fun -option-with-argument abc,def ( hash OPTIONS )

Then indicated argument gets the next argument.

    > fun -abc aaa -def bbb -ghi
    -abc
    aaa
    -def
    bbb
    -ghi
    -ghi

# 1.9 class

A class is almost same as a function.
    
A class is added with "class" inner command to current object.

    > class klass ( print aaa\n )
    > klass
    aaa

    > class klass ( ls | ary AAA )
    > object a
    > a::run klass

    "AAA" array is added to "a" object.

If "main" object is defined in the class, xyzsh called the item when command name is the object.

    > class A ( def main ( print called\n ) )
    > object a ( A )
    > a
    called

# 2.0 Command replacement

Command replacement pastes output of block to command line

    > print $(ls)
    main.c sub.c

    > print `ls`
    main.c sub.c

You can use $(...) or `...` for expanding each line as one argument.

    > def fun ( ary -size ARGV )

    > fun $(print aaa\nbbb\nccc\n )
    3

If you want to paste all output as one argument, you can use "$(...)"

    > print "$(split -target "aaa bbb ccc")"
    aaa
    bbb
    ccc

    > fun "$(print aaa\nbbb\nccc\n )"
    1

You can use $a(), $m() and $w() for changing the termination of line.

    > def fun ( ary -size ARGV )

    > fun $a(print aaa bbb ccc | split -La)
    3

    > fun $m(print aaa bbb ccc | split -Lm)
    3

    > fun $w(print aaa bbb ccc | split -Lw)
    3

If you want to reffer option by command replacement, use $-().

    > print "1234512345" | index -count 2 1
    5
    > print "1234512345" | index $(print "-count") 2 1
    invalid command using
    > print "1234512345" | index $-(print "-count") 2 1
    5

# 2.1 Exception

"try" inner command treats exeception.If error ocurrs on the first block of try, immediately runned the second block.
You can see the error message with context pipe of the second block

    > try ( print ) catch ( |=~ "invalid command using" && print "get invalid command using" )

"raise" inner command makes an error.

    > try ( make || raise "failed on make"; sudo make install) ( print make failed\n )

# 2.2 Refference

"ref" inner command treats refference.
You can output the address of the objects using below.

ref (object name)

    > ls | var a b c; ref a b c
    0x12345678
    0x23456789
    0x34567890

On the opposite

    > print 0x12345678 | ref x

You can bind the address to the object.

So

    > ls | var a b c; ref a b c | ref x y z

This makes that a is same to x, b is same to y, c is same to z.

xyzsh difend the addresses and denys the invalid adresses.

# 2.3 external commands

External command objects is rehashed to sys object on xyzsh initialization.
When running rehash, xyzsh initials external command objects on sys objects.

If you want to call "dmesg" external commands, run

    > sys::dmesg

However, fulequently used commands are entried on root object with refference.

So you can use ls without sys::

    > ls

If you want to entry new external program for root object, you may add new program name to ~/.xyzsh/program like below and run rehash inner command.

    > print dmesg\n >> ~/.xyzsh/program
    > rehash
    > dmesg

If you want to entry all external programs for root object, you can run "install_all_external_program_to_root_object" function.

# 2.4 subshell

You can use some statments as one statment using subshell.

    (pwd; echo aaa ; print bbb\n) | less

Subshell of (pwd, echo aaa, print bbb\n) outputs next command(less)

Context pipe in subshell has front command output

    pwd | ( | less; | less; | sub -global . (|uc))

"less" is runned twice getting output of pwd, and output of pwd is upper cased.

# 2.5 Comment

From # to end of line is comment.

    > ls -al     # --> Output of file list

# 2.6 Tilda expression

Almost same as bash

# 2.7 Glob expression
    
Almost same as bash. Using glob(3).

# 2.8 here document

Here document is useful for string which has line fields.

    > print <<<EOS
    aaaa
    bbbb
    cccc
    EOS | less

From the next line after <<<EOS to the line which head of line is EOS is treated as one string.
less gets
"aaaa
bbbb
cccc"

However

    print <<<EOS | less
    aaaa
    bbbb
    cccc
    EOS

can not be runned like bash.

You can expand variables

    > ls | var a b c; 
    print <<<EOS
    $a
    $b
    $c
    EOS | less
    
    less gets
    $a contents
    $b contents
    $c contents

If you do not want to expand variables, you use below

    > print <<<'EOS'
    $a
    $b
    $c
    EOS | less

# 2.9 global pipe
    
You can use global pipe writing |> at tail of statment.

    > ls |>

You can read global pipe writing |> at head of statment.

    > |> print
    main.c
    sub.c
    sub2.c
    > |> print
    main.c
    sub.c
    sub2.c

If you want to append data to global pipe, you can use |>>

    > print aaa\n |>
    > print bbb\n |>>
    > |> print
    aaa
    bbb

# 3.0 line context pipe

Indicate line number after context pipe for reading line number from context pipe.

    > print aaa bbb ccc | split | (|1 print)
    aaa

    > print aaa bbb ccc | split | (|2 print)
    aaa
    bbb

    > print aaa bbb ccc | split | (|1 print; |1print; |1print; |1print)
    aaa
    bbb
    ccc
    return code is 262

    > print aaa bbb ccc | split | (|-1 print )
    aaa
    bbb
    ccc

    > print aaa bbb ccc | split | (|-2 print )
    aaa
    bbb

    > print aaa bbb ccc | split | (|-2 print; |-1 print )
    aaa
    bbb
    ccc

# 3.1 redirect

You can use redirect with <
    
    > ls | write AAA
    > cat <AAA
    main.c sub.c fix.c

# 3.2 alias

alias of xyzsh is not the same as alias of bash.

If you want to define "ls" alias as "ls -al", define like below.

    alias ls ( sys::ls -al )

Below definition occurses an error on runtime.

    alias ls ( ls -al )

For expample of alias

    > alias ls ( sys::ls -al )
    > ls src/main.c
    -rw-r--r--  1 ab25cq  staff  2407  7  7 14:04 src/main.c

3.2 error output

To write error output to file, use "write" inner command and "-error" option.

    > ./output-error-program 
    data
    error-data

    > ./output-error-program | write -error data-from-error
    data

    > ./output-error-program | write data
    error-data

    > ./output-error-program | write -error data-from-error > /dev/null

    To read error output, use "print" inner command and "-read-from-error" option.

    > ./output-error-program | print -read-from-error
    error-data

    > ./output-error-program | (|print -read-from-error; | print ) | less
    error-data
    data

    > ./output-error-program | (|print -read-from-error | less; | print | less)
    error-data
    data

# 3.3 treatment multi line text

If you want to treat multi line with xyzsh, use -La option.
With -La option, text processing inner commands treat \a as a line field, then you can treat \n,\r\n,\r as normal text to pocess multi line.
It makes code complicated, but you may be able to write code debugging with p inner command.

# 3.4 Interactive shell

xyzsh uses readline for command line completion, and you can almost use it like bash.

You can use macro with typing C-x. It can paste the result of xyzsh one line script to command line. Edit ~/.xyzsh/macro for your original macro.

# 3.5 MANUAL

typeof (object name)
-
Output object kind of object of argument.
Output is one of blow

    "var"
    "array"
    "hash"
    "list"
    "native function"
    "block"
    "file dicriptor"
    "file dicriptor"
    "job"
    "object"
    "function"
    "class"
    "external program"
    "completion"
    "external object"
    "alias"

alias name|name (block)
-
    > alias ls ( sys::ls -al )
    > self | egrep ^ls
    ls: alias
    > ls src/main.c
     -rw-r--r--  1 ab25cq  staff  2407  6 21 19:40 src/main.c

    > alias ls ( inherit -al )
    > self | egrep ^ls
    ls: alias
    > ls src/main.c
     -rw-r--r--  1 ab25cq  staff  2407  6 21 19:40 src/main.c

    "alias name" outputs a definition of alias.

    > alias ls
    sys::ls -al

expand_tilda
-
A filter expanding tilda of path name. 

    > print /home/ab25cq/abc | expand_tilda | pomch
    /home/ab25cq/abc

    > print ~/abc | expand_tilda | pomch
    /home/ab25cq/abc

    > print ~root/abc | expand_tilda | pomch
    /root/abc
    -
    defined (object name)
    -
    if object is defined, return true

    > object ABC
    > co ABC
    > ls | var A B C
    > if(defined A) ( print yes\n ) ( print no\n )
    yes
    > if(defined self::D) ( print yes\n ) ( print no\n )
    no
    > if(deifned ::A) (print yes\n ) ( print no \n)
    no

kanjicode
-
show language setting

    > kanjicode   # default of xyzsh kanjicode is byte
    byte
    > kanjicode -utf8  # set
    > kanjicode
    utf8

    -byte change language setting to byte
    -sjis change language setting to SJIS(for Japanese)
    -eucjp change language setting to EUCJP(for Japanese)
    -utf8 change language setting to UTF8

funinfo
-
output running function or class information.

    > funinfo
    run time error
    xyzsh 1: [funinfo] invalid command using
    return code is 8192
    > def fun ( funinfo )
    > fun
    source name: xyzsh
    source line: 1
    run nest level: 2
    current object: root
    reciever object: root
    command name: fun

jobs
-
lists suspended jobs.

    > vim help.xyzsh # will be pressed CTRL-Z
    > jobs
    [1] vim help.xyzsh (pgrp: 46302)
    > fg

fg (job number)
-
forground a suspended job.

    > vim help.xyzsh # will be pressed CTRL-Z
    > jobs
    [1] vim help.xyzsh (pgrp: 46302)
    > fg

exit
-
exited from xyzsh.

    -force If there are some suspended jobs, exited from xyzsh.

    > vim help.xyzsh # will be pressed CTRL-Z
    > jobs
    [1] vim help.xyzsh (pgrp: 46302)
    > exit
    run time error
    xyzsh 1: [exit] jobs exist
    return code is 8192
    > exit -force

while (block1) (block2)
-
    while the return code of block1 is true, continue to run block2.

    > print 0 | var -local I; while(I | -lt 5) ( print $I\n; ++ I )
    0
    1
    2
    3
    4
    5
    -
    for (variable name) in (argument1) (argument2)... (block1)
    -
    Block performs, as long as there is an argument each time.
    ex)
    > for i in a b c ( print $i \n )
    a
    b
    c
    > for i in $(seq 1 3) ( print $i \n )
    1
    2
    3

break
-
exited from while or each, for, times loop.

    > split -target "aaa bbb ccc" | each ( | print; | if(| chomp | = bbb) ( break ))
    aaa
    bbb

true
-
return the rusult of true.

    > if(true) ( print yes\n) else ( print no\n )
    yes

false
-
return the result of false.


    >if(false) ( print yes\n ) else ( print no\n)
    no
    -
    if (condition1) (block1) (condition2) (block2)...(conditioni X) (block X) (the last of blocks)
    -
    If the result of condition is true, run conresponding block. If the all result of conditions, runthe last of blocks.

    > if(false) ( print 1\n ) elif(false) ( print 2\n ) else if (false ) ( print 3\n ) elsif (false) ( print 4\n) else ( print 5\n )
    5

    > ls | each ( | if(| chomp | -d) ( print found a directory\n ) )
    found a directory
    found a directory
    found a directory

    > split -target "aaaaa bbb cccccc ddd e" | each ( | if(|chomp|=~ ^...\$) ( | print ))
    bbb
    ddd

return (return code)
-
    If running function or class, exited from it.

    > def fun ( print head\n; return 1; print tail)
    > fun
    head
    return code is 1

stackinfo
-
    show the information of xyzsh stack which is used for memory management.

    > stackinfo
    slot size 32
    pool size 128
    all object number 4096

stackframe
-
    list of local variable difinitions.

    > stackframe
    ARGV: array

    > def fun ( ls | var -local A B C; stackframe )
    > fun
    C: var
    B: var
    A: var
    OPTIONS: hash
    ARGV: array

gcinfo
-
    show the information of xyzsh gabage colector which is used for memory management.

    > gcinfo
    free objects 3902
    used objects 4285
    all object number 8192
    slot size 64
    pool size 128

subshell (block)
-
    run the block. The context pipe in the block has previous command result or standard input.
    You can omit the command name.

    > subshell (print a\n; print b\n; print c\n ) | less
    a
    b
    c
    > (print a\n; print b\n; print c\n) | less
    a
    b
    c
    > print aaa\n | (| print; | print ; | print)
    aaa
    aaa
    aaa

sweep (object name1) (object name2) ... (object name X)
-
    Remove the objects in current object. If you omit the arguments, run the gabage collection.

    > ls | var A B C
    > sweep A
    > print $A\n
    run time error
    xyzsh 1: [print] no such as object(A)
    > sweep          # run gabage collection
    47 objects deleted

print (string1) (string2) ... (string X)
-
    Outputs the strings
    If you use this for filter, outputs the inout then.

    -error Outputs to error output.
    -read-from-error This option is enable in filter. Get from error output and output to standard output.

    > print "Hello World\n"
    Hello World

    > split -target "aaa bbb ccc" | each ( | print )
    aaa
    bbb
    ccc

    > print "" | print
    return code is 16384

    > vim main.c
    #include <stdio.h>

    int main() {
        printf("Hello World\n");
        exit(0);
    }

    > cat main.c | while(|1 print |>) (|> join ""; )
    #include <stdio.h>

    int main() {
        printf("Hello World\n");
        exit(0);
    }

    > (print aaa\n; print -error error\n )
    aaa
    error

    > vim a.c
    #include <stdio.h>
    #include <stdlib.h>

    int main() {
        fprintf(stdout, "OUTPUT\n");
        fprintf(stderr, "ERROR\n");
        exit(0);
    }
    > gcc a.c
    > ./a.out | (|print -read-from-error | less; | less)
     --- less ---
    EROOR

     --- less ---
    OUTPUT

load (script file name) (argument1) (argument2) ... (argument X)
-
Run the script file. Local variable stackframe is initialized, and local variable array "ARGV" has the arguments.

    -dynamic-library load C language extension library. Searched path is 3 step. First searched it from /usr/local/xyzsh/(pathname) or (installed prefix path)/(pathname), next searched it from ~/lib/(pathname), and finaly searched it from (absolute pathname or relative pathname). You can't ommit the file name extesion.

    > vim a.xyzsh
    print Hello Script\n

    > load a.xyzsh
    Hello Script

    > vim a.xyzsh
    ARGV

    > load a.xyzsh A B C
    A
    B
    C

    > load -dynamic-library [TAB]
    migemo.so

    > load -dynamic-library migemo.so

eval (block|string)
-

    Run the block or the string.
    If you use this for filter, run the getting input.

    > eval "print Hello\n"
    Hello

    > print ls | var A; eval "$A -al"
    output of "ls -al"

    > print "ls -al" | eval
    output of "ls -al"

    > cat src/main.c | eval "| uc |less"
    output of "cat src/main.c|uc"

msleep (number)
-

    Stop to run while the number minits.

    > msleep 10
    .oO.oO.oO.oO.oO.oO.oO.oO.oO
    -
    raise (error message)
    -
    occur a error.

    > make && raise "make is failed"
    run time error
    xyzsh 1: [raise] make is failed
    return code is 8192

    > try (
        make || raise "make error"
        sudo make install || raise "sudo make install error"
    ) catch ( 
        | =~ "make error" && print "catch make error\n";
        | =~ "sudo make install error" && print "catch make install error\n"
    )
ablock (number)
-

    On running function, output the argument block source.
    If you omit the number, assume that the number is 0.

    -run Run the argument block.
    -number Output the number of argument blocks.

    > def fun ( ablock | pomch )
    > fun ( times 3 ( pwd ) )
    times 3 ( pwd )

    > def fun ( ablock -run )
    > fun ( times 3 ( pwd ) )
    /Users/ab25cq
    /Users/ab25cq
    /Users/ab25cq

    > def fun ( ablock -run 1 )
    > fun ( pwd ) ( whoami ) ( sys::groups )
    ab25cq

    > def fun ( ablock -number )
    > fun ( pwd ) ( whoami ) ( sys::groups )
    3

time (block)
-
    Run the block and mesure the time of running.

    > time ( sleep 1 )
    1 sec(0 minuts 1 sec)

    > time ( sleep 3 )
    3 sec(0 minuts 3 sec)
    -
    umask (number)
    -
    umask.

    > touch aaa
    > ls -al aaa
    -rw-r--r--  1 ab25cq  staff  0  3 11 15:02 aaa

    > umask 000
    > touch bbb
    > ls -al bbb
    -rw-rw-rw-  1 ab25cq  staff  0  3 11 15:03 bbb

    > umask 777
    > touch ccc
    > ls -al ccc

rehash
-
    Searching the directory in PATH environment variable, store the external objects to ::sys object.

    > print $PATH:$HOME/bin | export PATH
    > rehash

    > groups
    run time error
    xyzsh 1: [groups] command not found
    return code is 127
    > sys::groups
    staff com.apple.access_screensharing com.apple.sharepoint.group.1 everyone _appstore localaccounts _appserverusr admin _appserveradm _lpadmin _lpoperator _developer
    > print groups\n >> ~/.xyzsh/program
    > rehash
    > groups
    staff com.apple.access_screensharing com.apple.sharepoint.group.1 everyone _appstore localaccounts _appserverusr admin _appserveradm _lpadmin _lpoperator _developer

try (block1) (block2)
-
    When occured a error running block1, imediately run block2. To get error message context pipe in block2, and to occure error in block1 use "raise" inner command.
    -
    errmsg
    -
    Output error message.

    > groups
    run time error
    xyzsh 1: [groups] command not found
    return code is 127
    > errmsg
    xyzsh 1: [groups] command not found
    -
    prompt (block)
    -
    Set interactive shell prompt with block output.

    > prompt ( print "ab25cq's terminal > " )
    ab25cq's terminal > 

rprompt (block)
-
    Set interactive shell right prompt with block output.

    > rprompt ( pwd )

each (block)
-
    Run the block with each line. To get a line, use context pipe.

    -number (number) It performe (number) lines at a time
    -Lw Processing with CRLF line field
    -Lm Processing with CR line field
    -Lu Processing with LF line field
    -La Processing with BEL line field

    > split -target "01aaa 02bbb 03ccc 04ddd 05eee" | each ( | print )
    01aaa
    02bbb
    03ccc
    04ddd
    05eee

    > print "aaa\nbbb ccc\nddd eee" | split -La " " | each -La ( |chomp| less )
     --- less ---
    aaa
    bbb
     ------------

     --- less ---
    ccc
    ddd
     ------------

     --- less ---
    eee
     ------------

    > split -target "01aaa 02bbb 03ccc 04ddd 05eee" | each -number 2 ( | join )
    01aaa 02bbb
    03ccc 04ddd
    05eee

join (filed string)
-
    Make one line string from multi line string. If you omit field string, xyzsh set space on it.

    -Lw Processing with CRLF line field
    -Lm Processing with CR line field
    -Lu Processing with LF line field
    -La Processing with BEL line field

    > print "aaa bbb ccc" | split | join 
    aaa bbb cccc

    > print "aaa bbb ccc" | split | join +
    aaa+bbb+ccc

    > ls | join ,
    AUTHORS,CHANGELOG,LICENSE,Makefile,Makefile.in,README,README.ja,USAGE,USAGE.ja,a.xyzsh,aaa,bbb,ccc,completion.xyzsh,config.h,config.h.in,configure,configure.in,ddd,eee,help.xyzsh,install.sh,libxyzsh.1.7.1.dylib,libxyzsh.1.dylib,libxyzsh.dylib,man,read_history.xyzsh,src,xyzsh,xyzsh.dSYM,xyzsh.xyzsh,

    > print "aaa\nbbb ccc\nddd eee" | split -La " " | join -La +
    aaa
    bbb+ccc
    ddd+eee

lines (line number) (block) (line number) (block), ..., (line number) (block)
-
    Run block with each indicated line number. Context pipe in block has the line string.
    You can use range for line number
    (line number1)..(line number2)
    Line number begins from 0. <0 is counted from tail.
    A case of (line number1) > (line number2), reverse the order.

    -Lw Processing with CRLF line field
    -Lm Processing with CR line field
    -Lu Processing with LF line field
    -La Processing with BEL line field

    > split -target "aaa bbb ccc ddd eee" | lines 0 1 2
    aaa
    bbb
    ccc

    > split -target "aaa bbb ccc ddd eee" | lines 0 0 0
    aaa
    aaa
    aaa

    > split -target "aaa bbb ccc ddd eee" | lines -1 -2
    eee
    ddd

    > split -target "aaa bbb ccc ddd eee" | lines 4..0
    eee
    ddd
    ccc
    bbb
    aaa

    > split -target "aaa bbb ccc ddd eee" | lines -1..0
    eee
    ddd
    ccc
    bbb
    aaa

    > split -target "aaa bbb ccc ddd eee" | lines 0..1 (| chomp | add XXX | pomch ) 2..-1 ( | uc )
    aaaXXX
    bbbXXX
    CCC
    DDD
    EEE

    > split -target "aaa bbb ccc ddd eee" | ( | lines 0..1 | join; | lines 2..-1 | join )
    aaa bbb
    ccc ddd eee

sort (blovk)
-
sort filter. Context pipe in block has two lines which is left data and right data.
In the block, compare left data with right data, and the return code is used by sort.

ex) > ls | sort ( | var a b; a | -slt $b )

    -shuffle randomize sort. Omit the block.
    -Lw Processing with CRLF line field
    -Lm Processing with CR line field
    -Lu Processing with LF line field
    -La Processing with BEL line field

    > split -target "ddd eee aaa ccc bbb" | sort ( | var a b; a | -slt $b )
    aaa
    bbb
    ccc
    ddd
    eee

    > spit -target "ddd eee aaa ccc bbb" | sort (| var a b; a | -sgt $b )
    eee
    ddd
    ccc
    bbb
    aaa

    > split -target "ddd eee aaa ccc bbb" | sort -shuffle    # random sort
    bbb
    eee
    ddd
    aaa
    ccc

    > split -target "ddd eee aaa ccc bbb" | sort -shuffle | head -n 2
    ddd
    bbb

-n 
-
if input exists, return code is 0 (which represents true)

-z 
-
if input doesn't exist, return code is 0

-b 
-
if the file exists and it's a block special file, return code is 0

-c 
-
if the file exists and it's a chalacter special file, return code is 0

-d 
-
if the file exists and it's a directory, return code is 0

-f
-
if the file exists and it's a regular file, return code is 0

-h 
-
no implement

-L 
-
if the file exists and it's a sibolic link, return code is 0

-p 
-
if the file exists and it's a named pipe, return code is 0

-t
-
no implement

-S
-
if the file exists and it's a socket, return code is 0

-g
-
if the file exists and it's setted GID, return code is 0

-k
-
if the file exists and it's setted sticky bit, return code is 0

-u
-
if the file exists and it's setted SUID, return code is 0

-r
-
if the file exists and it's readable, return code is 0

-w
-
if the file exists and it's writable, return code is 0

-x
-
if the file exists and it's excutable, return code is 0

-O
-
if the file exists and it's owned by the user, return code is 0

-G
-
if the file exists and it's owned by the group, return code is 0

-e
-
if the file exists , return code is 0

-s
-
if the file exists and the size is begger, return code is 0

= (argument)
-
the input equals to the argument, return code is 0

    -ignore-case ignore case

!= (argument)
-
the input doesn't equals to the argument, return code is 0

    -ignore-case ignore case

-slt (argument)
-
the input is smaller than argument as string, return code is 0

    -ignore-case ignore case

-sgt (argument)
-
the input is bigger than argument as string, return code is 0

    -ignore-case case

-sle (argument)
-
the input is smaller than argument or equals to the argument as string, return code is 0

    -ignore-case ignore case

-sge (argument)
-
the input is bigger than argument or equals to the argument as string, return code is 0

    -ignore-case ignore case

-eq (argument)
-
If the input equals to the argument as numeric

-ne (argument)
-
If the input doesn't equal to the argument as numeric

-lt (argument)
-
If the input is smaller than the argument as numeric

-le (argumrnt)
-
If the input is smaller than the argumrnt or equals to the argument as numeric

-gt (argument)
-
If the input is bigger than the argument as numeric

-ge (argumrnt)
-
If the input is bigger than the argumrnt or equals to the argument as numeric

-nt (argumrnt)
-
If the input is newer than the argumrnt

-ot (argumrnt)
-
If the input is older than the argumrnt

-ef (argument)
-
If inode of the input file equals to inode of the argument file

=~ (argument regex)
-
Filter for regex. If the regex maching, return code is 0(true).
Setted global variable blows.

Group matching string --> 1,2,..,9
Matched string --> 0, MATCH
Premached string --> PREMATCH
Postmatched string --> POSTMATCH
Lastmatched string --> LAST_MATCH
Matched number --> MATCH_NUMBER

    -offsets Output all maching points.
    -verbose Output maching point(index).
    -ignore-case Ignore case.
    -multi-line Allow to write multiline matching regex, but the performance is less than normal.
    -byte assume text encode as byte code.
    -utf8 assume text encode as utf-8 code.
    -sjis assume text encode as SJIS code.
    -eucjp assume text encode as EUCJP code.

    > print abcdefghijklmn | =~ e
    > PREMATCH
    abcd
    > MATCH
    e
    > POSTMATCH
    fghijklmn

    > print abcdefghijklmn | =~ -verbose e
    4

    > print abcdefghijklmn | =~ z
    return code is 4089

    > print aaabbbcccdddeee | =~ '^(...)...(...)'
    > PREMATCH

    > MATCH
    aaabbbbccc
    > POSTMATCH
    dddeee
    > 1
    aaa
    > 2
    ccc
    > LAST_MATCH
    ccc
    > MATCH_NUMBER
    3

    > print aaabbbcccdddeee | =~ '^(...)...(...)' -offsets
    0       # all matching offsets
    9
    0       # group 1 matching offsets
    3
    6       # group 2 matching offsets
    9
-
selector
-
select line by user and output it.

    Key manipulation:
    cursor key -> moving cursor
    CTRL-D, CTRL-U -> scroll
    a -> reverse all marks
    ENTER -> select
    q, CTRL-c -> cansel

Key manipulation of selector is almost same as less(3).

    -multiple Allow to select multi line with space-key
    -preserve-position no initialize cursor position and scroll top position
    -Lw Processing with CRLF line field
    -Lm Processing with CR line field
    -Lu Processing with LF line field
    -La Processing with BEL line field
    -byte assume text encode as byte code.
    -utf8 assume text encode as utf-8 code.
    -sjis assume text encode as SJIS code.
    -eucjp assume text encode as EUCJP code.

    > ls | selector
    AUTHORS

    > ls | selector -multiple
    AUTHORS
    CHANGELOG
    main.c

p
-
View pipe contents. Allow to pass with pressing ENTER KEY, and occure error with pressing CTRL-C, 'q' KEY.
This is for debuging.

Key manipulation of p is almost same as less(3).

    -preserve-position No initialize cursor position and scroll top position
    -byte assume text encode as byte code.
    -utf8 assume text encode as utf-8 code.
    -sjis assume text encode as SJIS code.
    -eucjp assume text encode as EUCJP code.

    > ls | p
    run time error
    xyzsh 1: [p] p: canceled
    return code is 8192

    > ls | p
    AUTHORS
    CHANGELOG
    LICENSE
    Makefile
    Makefile.in
    README
    README.ja
    USAGE
    USAGE.ja
    a.xyzsh
    completion.xyzsh
    config.h
    config.h.in
    configure
    configure.in

write (file name)
-
Write data in pipe to file.

    -append append to the file.
    -force allow to override
    -error write error output. keep and continue outputing from input

    > touch aaa
    > ls | write aaa
    run time error
    xyzsh 1: [write] The file exists. If you want to override, add -force option to "write" runinfo
    return code is 8192
    > ls | write -force aaa

    > vim a.c
    #include <stdio.h>
    #include <stdlib.h>

    int main() {
        fprintf(stderr, "hello world\n");
        exit(0);
    }

    > gcc a.c
    > ./a.out | write -error a
    > cat a
    hello world

cd (directory name)
-
Change current directory.
If you omit the directory name, change current directory to home directory.

    > pwd
    /Users/ab25cq
    > mkdir abc
    > cd abc
    > pwd
    /Users/ab25cq/abc

pushd (directory name)
-
Save argument directory to directory stack.

    > pwd
    /Users/ab25cq
    > pushd .
    > cd /
    > pwd
    /
    > popd
    > pwd
    /Users/ab25cq

popd 
-
Restore directory from directory stack.

    > pwd
    /Users/ab25cq
    > pushd .
    > cd /
    > pwd
    /
    > popd
    > pwd
    /Users/ab25cq

++ (variable name)
-
Assume variable as numeric, and plus 1.

    > print 1 | + 1 | var I
    > print $I\n
    2
    > ++ I
    > print $I\n
    3
    > I
    3

-- (variable name)
-
Assume variable as numeric, and minus 1.

    > print 1 | + 1 | var I
    > I
    2
    > -- I
    > I
    1

\+ (number)
-
Assume pipe data as numeric, and plus the argument.

    > print 3 | + 1 
    4

\- (number)
-
Assume pipe data as numeric, and minus the argument.

    > print 3 | - 1
    2

\* (number)
-
Assume pipe data as numric, and multiple the argument.

    > print 3 | * 3
    9

\/ (number)
-
Assume pipe data as numric, and divide the argument.

    > print 1 | / 2
    0
    > print 5 | / 2
    2
    > print 5 | / 0
    run time error
    xyzsh 1: [/] zero div
    return code is 8192

mod (number)
-
Assume pipe data as numric, and mod the argument.

    > print 1 | mod 2
    1

pow (number)
-
Assume pipe data as numric, and pow the argument.

    > print 2  | pow 2

abs
-
Assume pipe data as numric, and output absolute number.

    > print 2 | abs
    2
    > print -2 | abs
    2

def (function name) (block)
-
Entry function with block. If you use this for filter, entry function with pipe data.
If you omit the block, output the function source.

    -inherit If same name function exists, get the older function as parent. You can call the parent function with "inherit" inner command.
    -option-with-argument (argument name,argument name, ..., argument name) Set the argument as getting string argument.
    (ex) > def fun -option-with-argument abc,def ( hash OPTIOINS ); fun -abc aaa -def bbb
    -abc
    aaa
    -def
    bbb
    -copy-stackframe When making new stackframe, xyzsh copys variables in old stackframe to new one.
    (ex)
    > vim a.xyzsh
    class times -copy-stackframe ( 
        print 0 | var -local _i
        while(_i | -lt $ARGV[0]) (
           | eval $(block)
           ++ _i
        )
    )
    > load a.xyzsh; print Hello | var -local a; times 3 ( a )
    Hello
    Hello
    Hello

    If there is not -copy-stackframe option, xyzsh raises an error which is "not found a variable" on "times 3 ( a )" code.

    > def fun ( times 3 ( echo Hello ) )
    > fun
    Hello
    Hello
    Hello

    > print "times 3 ( echo Hello )" | def fun 
    > fun
    Hello
    Hello
    Hello

    > def fun | pomch
    times 3 ( echo Hello )

    > def fun ( echo Hello )
    > def fun -inherit ( inherit; echo Hello2; ) 
    > fun
    Hello
    Hello2

    > def fun -option-with-argument abc,def ( hash OPTIOINS )
    > fun -abc aaa -def bbb ccc
     -abc
     aaa
     -def
     bbb
     ccc
     ccc

    > def Fun ( split -target "AAA BBB CCC" | var A B C )
    > class Klass ( split -target "DDD EEE FFF" | var D E F )
    > object a
    > a::run ( Fun )
    > a::A
    run time error
    xyzsh 1: [A] there is not this object
    > a::B
    run time error
    xyzsh 1: [B] there is not this object
    > a::C
    run time error
    xyzsh 1: [C] there is not this object
    > A
    AAA
    > B
    BBB
    > C
    CCC
    > a::run ( Klass )
    > a::D
    DDD
    > a::E
    EEE
    > a::F
    FFF

class (class name) (block)
-
Entry class with block. If you use this for filter, etnry class with pipe data.
If you omit the block, output the class source.

    -inherit If same name class exists, get the older class as parent. You can call the parent class with "inherit" inner command.
    -option-with-argument (argument name,argument name, ..., argument name) Set the argument as getting string argument.
    (ex) > class klass -option-with-argument abc,def ( hash OPTIOINS ); klass -abc aaa -def bbb
    -abc
    aaa
    -def
    bbb

    > class klass ( times 3 ( echo Hello ) )
    > klass
    Hello
    Hello
    Hello

    > print "times 3 ( echo Hello )" | class klass 
    > klass
    Hello
    Hello
    Hello

    > class klass | pomch
    times 3 ( echo Hello )

    > class klass ( echo Hello )
    > class klass -inherit ( inherit; echo Hello2; ) 
    > klass
    Hello
    Hello2

    > class klass -option-with-argument abc,def ( hash OPTIOINS ); klass -abc aaa -def bbb ccc
     -abc
     aaa
     -def
     bbb
     ccc
     ccc

    > def Fun ( split -target "AAA BBB CCC" | var A B C )
    > class Klass ( split -target "DDD EEE FFF" | var D E F )
    > object a
    > a::run ( Fun )
    > a::A
    run time error
    xyzsh 1: [A] there is not this object
    > a::B
    run time error
    xyzsh 1: [B] there is not this object
    > a::C
    run time error
    xyzsh 1: [C] there is not this object
    > A
    AAA
    > B
    BBB
    > C
    CCC
    > a::run ( Klass )
    > a::D
    DDD
    > a::E
    EEE
    > a::F
    FFF

inherit (argument|option|block)
-
Call parent class or function or inner command.

    > def fun ( echo Hello )
    > def fun -inherit ( echo Hello2; inherit )
    > fun
    Hello2
    Hello

var (variable name1) (variable name2),..., (variable name X)
-
If you don't use this for filter, output the variable contents.
If you use this for filter, split with line fields, and get from first line by turns to each variables.

    -new create a new object and output the address
    -local Treat as local variable.
    -shift If you use this for filter, outputs and substitutes the pipe data at same time.

    > split -target "ABC DEF GHI" | var A B C
    > var A
    ABC
    > A
    ABC
    > var A B C
    ABC
    DEF
    GHI

    > print ABC | var -new | ref X
    > var X
    ABC
    > X
    ABC

    > split -target "ABC DEF GHI" | var -shift A
    DEF
    GHI
    > var A
    ABC

    > split -La -target "ABC\nDEF GHI\nJKL MNO\nPQR" " " | var -La A B C
    > var A
    ABC
    DEF
    > var B
    GHI
    JKL
    > var C
    MNO
    PQR

object (object name1) (object name2), ..., (object name X) (block)
-
Create objects. If a block exists, initialize the objects with block.

    -new create a new object and output the address
    -local create a new object as a local variable

    > object a
    > a::run ( split -target "AAA BBB CCC" | var A B C)
    > a::A
    AAA
    > a::B
    BBB
    > a::C
    CCC

    > object a ( split -target "AAA BBB CCC" | var A B C)
    > a::A
    AAA
    > a::run ( A )
    AAA
    > a::run ( var A )
    AAA

    > class Human ( | var Name Age; def show ( var Name Age | printf "name:%s\nage:%s\n" ) )
    > object ab25cq ( split -target "ab25cq 35" | Human )
    > ab25cq::Name
    ab25cq
    > ab25cq::Age
    35
    > ab25cq::show
    name:ab25cq
    age:35

run (class name|block)
-
If a block exists, run the block with message passed object as current object.
If class names exist, run the class with message passed object as current object.

    > object a
    > a::run ( split -target "AAA BBB CCC" | var A B C)
    > a::A
    AAA
    > a::B
    BBB
    > a::C
    CCC

    > object a ( split -target "AAA BBB CCC" | var A B C)
    > a::A
    AAA
    > a::run ( A )
    AAA
    > a::run ( var A )
    AAA
    -
    times number (block)
    -
    run a block number times

    > times 3 ( print Hello World\n )
    Hello World
    Hello World
    Hello World

pwo
-
Show current object.

    > pwo
    root
    > object obj
    > co obj
    > pwo
    root:obj
    > co parent
    > pwo
    root

co (object name)
-
Change current object.

    > pwo
    root
    > object obj
    > co obj
    > pwo
    root:obj
    > co parent
    > pwo
    root

ref (variable name1) (variable name2), ..., (variable name X)
-
If you use for filter, bind address which is each lines in pipe data to the variable.
If you don't use for filter, output the address in variable.

    -local Treat as local variable.
    -shift Bind and output the address in the same time.
    -type If you use for fitler, Output a type of the address.
    -Lw Processing with CRLF line field
    -Lm Processing with CR line field
    -Lu Processing with LF line field
    -La Processing with BEL line field

    > split -target "aaa bbb ccc" | ary -new | ref X
    > ref X
    0x7fd73469c780
    > X
    aaa
    bbb
    ccc
    > ary X
    aaa
    bbb
    ccc

    > ls | var A B C

    > ref X A B C | ref -type
    array
    var
    var
    var

    > ref -type X A B C
    array
    var
    var
    var

    > ls | (|1 var -new; |1 var -new; |1 var -new )| ref -shift X
    0x7f8c5de9f9c0
    0x7f8c5de9f8c0

ary (variable name1) (variable name2), ... , (variable name X)
-
If you use this for filter, get from all pipe data to the variable.
If you don't use this for filter, output the variables.

    -new create a new object and output the address
    -local Treat as local variable.
    -index (number) When outputing variables, output only the index item of array.
    -insert (number) Append to the array at the index point. If the array doesn't exist, create new array.
    -append Append to the end of array. If the array doesn't exist, create new array
    -size Output the size of array.
    -Lw Processing with CRLF line field
    -Lm Processing with CR line field
    -Lu Processing with LF line field
    -La Processing with BEL line field

    > split -target "AAA BBB CCC" | ary A
    > ary A
    AAA
    BBB
    CCC
    > A
    AAA
    BBB
    CCC

    > ary A -index 0
    AAA
    > ary A -index 1
    BBB
    > ary A -index 2
    CCC
    > ary A -index -1
    CCC
    > ary A -index 3

    > print $A[0]\n
    AAA
    > print $A[1]\n
    BBB
    > print $A[-1]\n
    CCC
    > print $A[5]\n

    > split -target "DDD EEE FFF" | ary -append A
    > A
    AAA
    BBB
    CCC
    DDD
    EEE
    FFF

    > print "XXX" | ary -insert 1 A
    > A
    AAA
    XXX
    BBB
    CCC
    DDD
    EEE
    FFF

    > print "ZZZ" | ary -insert -2 A
    > A
    AAA
    XXX
    BBB
    CCC
    DDD
    EEE
    ZZZ
    FFF

    > ary -size A
    8

    > split -target "AAA BBB CCC" | ary -new | ref X
    > X
    AAA
    BBB
    CCC

    > split -La -target "AAA\nBBB CCC\nDDD EEE\nFFF" " "| ary -La X
    > X
    AAA
    BBB
    CCC
    DDD
    EEE
    FFF

    > ary X -index 0
    AAA
    BBB
    > ary X -index 1
    CCC
    DDD


hash (variable name1) (variable name2), ... , (variable name X)
-
If you use this for filter, get the all pipe data to a variable.
How to get the pipe data are
1st line --> key
2nd line --> item
3rd line --> key
.
.
.
an odd number line --> key
an even number line --> item
If you don't use for filter, output the hash.

    -new create a new object and output the address
    -local Treat as local variables.
    -append Append to the hash. If hash doesn't exist, create ia new hash.
    -size Output the size of hash.
    -key (key name) If you don't use for filter, output the hash item of key nly.
    -Lw Processing with CRLF line field
    -Lm Processing with CR line field
    -Lu Processing with LF line field
    -La Processing with BEL line field

    > split -target "key1 item1 key2 item2 key3 item3" | hash A
    > hash A
    key3
    item3
    key2
    item2
    key1
    item1

    > print $A[key1]\n
    item1
    > hash A -key key1
    item1
    > print $A[key4]\n

    > hash A -key key4

    > hash A -key key2
    item2

    > A | each -number 2 ( | lines 0 )
    key1
    key2
    key3

    > A | each -number 2 ( | lines 1 )
    item1
    item2
    item3

    > hash A -size
    3

    > print key4\nitem4 | hash -append A
    > A
    key4
    item4
    key3
    item3
    key2
    item2
    key1
    item1

export (environment varialbe name1) (environment variable name2), ... , (environment variable name X)
-
If you use this for filter, get lines from pipe data to each environment variables.
If you don't use this for filter, output the environment varible value.

    -shift If you use this for filter, output and get lines from pipe data to each environment variables.
    -Lw Processing with CRLF line field
    -Lm Processing with CR line field
    -Lu Processing with LF line field
    -La Processing with BEL line field

    > export PATH
    /bin:/usr/bin
    > print $PATH:/usr/local/bin | export PATH
    > export PATH
    /bin:/usr/bin:/usr/local/bin
    > split -target "AAA BBB CCC" | export A B -shift
    CCC
    > export A
    AAA
    > export B
    BBB
    > print $A\n
    AAA
    > print $B\n
    BBB

unset (environment varialbe name1) (environment variable name2), ... , (environment variable name X)
-
Remove environment variables.

    > print aaa | export AAA
    > env | grep AAA
    AAA=aaa
    > unset AAA
    > env | grep AAA
    return code is 1

quote
-
It is filter for quoting all non alphabet character except utf8-character.

    -byte assume text encode as byte code.
    -utf8 assume text encode as utf-8 code.
    -sjis assume text encode as SJIS code.
    -eucjp assume text encode as EUCJP code.

    > print "abcdefghij%&$]" | quote | pomch
    abcdefghij\%\&\$\]

length
-
It is a filter for outputing the pipe data size.

    -line-num output linefield number
    -byte assume text encode as byte code.
    -utf8 assume text encode as utf-8 code.
    -sjis assume text encode as SJIS code.
    -eucjp assume text encode as EUCJP code.
    -Lw Processing with CRLF line field
    -Lm Processing with CR line field
    -Lu Processing with LF line field
    -La Processing with BEL line field

    > print abc| length
    3
    > print abc\n | length
    4
    > print あいうえお | length -byte
    15
    > print あいうえお | length -utf8
    5
    > print abc\ndef\n | length -line-num
    2
    > print abc\ndef | length -line-num
    1
    > split -target "abc def ghi" | each ( | chomp | length )
    3
    3
    3

x (number)
-
It is a filter for increasing several times the pipe data.

    > print abc | x 2 | pomch
    abcabc

    > print abc\n | x 2 
    abc
    abc

    > split -target "abc def ghi" | each ( | chomp | x 2 | pomch )
    abcabc
    defdef
    ghighi

index (string)
-
It is a filter for outputing the index with searching the string position.

    -regex search with regex
    -quiet No output, but set return code.
    -ignore-case Ignore case.
    -multi-line Allow to write multiline matching regex, but the performance is less than normal.
    -number (number) Set the first position on searching string.
    -count 数値 Set the searching count.
    -byte assume text encode as byte code.
    -utf8 assume text encode as utf-8 code.
    -sjis assume text encode as SJIS code.
    -eucjp assume text encode as EUCJP code.

    > print あいうえお | index -utf8 う
    3
    > print abcabcabc | index c
    2
    > print abcabcabc | index -number 6 c
    8
    > print abcabcabc | index -count 2 c
    5
    > print abcabcabc | index -regex '..c'
    0
    > print abcabcabc | index -ignore-case BC
    1

rindex (string)
-
It is a filter for outputing the index with searching the string position.
Searching from the tail.

    -regex search with regex
    -quiet No output, but set return code.
    -ignore-case Ignore case.
    -multi-line Allow to write multiline matching regex, but the performance is less than normal.
    -number (number) Set the first position on searching string.
    -count 数値 Set the searching count.
    -byte assume text encode as byte code.
    -utf8 assume text encode as utf-8 code.
    -sjis assume text encode as SJIS code.
    -eucjp assume text encode as EUCJP code.

    > print あいうえおう | rindex -utf8 う
    5
    > print abcabcabc | rindex c
    8
    > print abcabcabc | index -number 6 c
    5
    > print abcabcabcabc | rindex -count 2 c
    8
    > print abcabcabc | rindex -ignore-case A
    6
    > print abcabcabc | rindex -regex '.b'
    6

lc 
-
It is a filter for outputing lower cased string in pipe data.

    -byte assume text encode as byte code.
    -utf8 assume text encode as utf-8 code.
    -sjis assume text encode as SJIS code.
    -eucjp assume text encode as EUCJP code.

    > print ABCDEFG | lc
    abcdefg

    > print あいうえおABCかきくけこ | lc -utf8
    あいうえおabcかきくけこ

uc 
-
It is a filter for outputing upper cased string in pipe data.

    -byte assume text encode as byte code.
    -utf8 assume text encode as utf-8 code.
    -sjis assume text encode as SJIS code.
    -eucjp assume text encode as EUCJP code.

    > print abcdefg | uc
    ABCDEFG

    > print あいうえおabcかきくけこ | uc -utf8
    あいうえおABCかきくけこ

chomp
-
It is a filter for removing last one line field. If no removing character, the return code is 1.

    > print ABC\n | chomp
    ABC
    > print ABC | chomp
    ABC
    return code is 1
    > split -target "ABC DEF GHI" | each ( | chomp )
    ABCDEFGHI

chop
-
It is a filter for removing last one character expcept that last one character is \r\n. If last one character is one character, it will remove two character. If no removing character, the return code is 1.

    -byte assume text encode as byte code.
    -utf8 assume text encode as utf-8 code.
    -sjis assume text encode as SJIS code.
    -eucjp assume text encode as EUCJP code.

    > print ABC | chop
    AB
    > print ABC\n | chop
    ABC
    > print ABC\r\n | chop
    ABC
    > print あいうえお | chop -utf8
    あいうえ

pomch
-
It is a filter for outputing string which is added line field to last.


    -Lu Processing with LF line field
    -La Processing with BEL line field

    > print ABC | pomch
    ABC
    > print ABC\n | pomch
    ABC
    return code is 1
    > split -target "ABC DEF GHI" | each ( |chomp | add XXX | pomch )
    ABCXXX
    DEFXXX
    GHIXXX

printf
-
    Like C language, output format string.
    However, getting arguments from pipe data.

    -Lw Processing with CRLF line field
    -Lm Processing with CR line field
    -Lu Processing with LF line field
    -La Processing with BEL line field

    > ls | head -n 3
    main.c
    sub1.c
    sub2.c

    > ls | printf "%s,%s,%s\n"
    main.c,sub1.c,sub2.c

    > split -target "1 2 3 4 5" | printf "(%d,%d)\n(%d,%d,%d)"
    (1,2)
    (3,4,5)

    > split -target "ABC\nDEF GHI\nJKL" " " -La | printf -La "(%s) (%s)"
    (ABC
    DEF) (GHI
    JKL)

sub (regex) (string|block)
-
Substitute strings at matching point to argument string or output of block.
In the block, context pipe has matching string.

If you use \ and digit or alphabet in the substitute string, it have the special meanings.
The global variables in the block have special string.

group strings \1..\9
(In the block 1,2,..,9)
maching string \&,\0
(In the block MATCH, 0)
Prematch string \`
(In the block PREMATCH)
Postmatch string \'
(In the block POSTMATCH)
Last matching string \+
(In the block LAST_MATCH)
Maching number
(In the block MATCH_NUMBER)

    > print abc | sub "a(.)c" "\1\1" | pomch
    bb

    The case of block, $1 - $9 local variable have the matching group string.

    > print abc | sub "a(.)c" ( var 1 1 )
    b
    b

    You can get the count of searching to use the local variable "SUB_COUNT"

    -no-regex Don't use regex for pattern. Use it as text.
    -ignore-case ignore case
    -multi-line Allow to write multiline matching regex, but the performance is less than normal.
    -global Allow to match at several times in a line.
    -quiet No output, but set return code and the local variables.
    -Lw Processing with CRLF line field
    -Lm Processing with CR line field
    -Lu Processing with LF line field
    -La Processing with BEL line field
    -byte assume text encode as byte code.
    -utf8 assume text encode as utf-8 code.
    -sjis assume text encode as SJIS code.
    -eucjp assume text encode as EUCJP code.

    > print abc | sub b B | pomch
    aBc
    > print abc | sub b ( | uc ) | pomch
    aBc
    > print abc | sub "a(.)c" '(\1\1)' | pomch
    (bb)
    > print abc | sub "a(.)c" ( var 1 1 | printf "(%s%s)" | pomch
    (bb)
    > print abc | sub b '(\0\0)' | pomch
    a(bb)c
    > print abc | sub b ( var 0 0 | printf "(%s%s)" ) | pomch
    a(bb)c
    > print abc | sub b '(\`\`)' | pomch
    a(aa)c
    > print abc | sub b ( var PREMATCH PREMATCH | printf "(%s%s)" ) | pomch
    a(aa)c
    > print abc | sub b "(\\\'\\\')" | pomch
    a(cc)c
    > print abc | sub b ( var POSTMATCH POSTMATCH | printf "(%s%s)" ) | pomch
    a(cc)c
    > print abcdefghij | sub '(.)c(.)' '(\+)' | pomch
    a(d)efghij
    > print abcdefghij | sub '(.)c(.)' ( var LAST_MATCH | printf "(%s)" ) | pomch
    a(d)efghij
    > print abcabcabc\n | sub -global b '' && SUB_COUNT
    acacac
    3
    > print abc | sub "a(.)c" ( var 1 1 )
    b
    b
    > print "a.b.c.d.e.f.g\n" | sub -no-regex -global . X
    aXbXcXdXeXfXg
    > print "ABCDEGHIJK\n" | sub -ignore-case c XX
    ABXXDEFGHIJK
    > split -target "AAA BBB CCC DDD EEE" | sub -multi-line AAA\nBBB\n XXX\n
    XXX
    CCC
    DDD
    EEE

scan (regex) (block)
-
    It is a filter for scanning pipe data with regex.
    If block exists, xyzsh run the block whenever data is matched.
    With block, context pipe has a maching string.

    You can get the count of matching to use the local variable "MATCH_COUNT".

    Blow global variables have special strings.

    Group matching string --> 1,2,..,9
    Matched string --> MATCH, 0
    Premached string --> PREMATCH
    Postmatched string --> POSTMATCH
    Lastmatched string --> LAST_MATCH
    Matched number --> MATCH_NUMBER

    -ignore-case ignore case
    -multi-line Allow to write multiline matching regex, but the performance is less than normal.
    -byte assume text encode as byte code.
    -utf8 assume text encode as utf-8 code.
    -sjis assume text encode as SJIS code.
    -eucjp assume text encode as EUCJP code.
    -Lw Processing with CRLF line field
    -Lm Processing with CR line field
    -Lu Processing with LF line field
    -La Processing with BEL line field

    > print abc | scan .
    a
    b
    c
    > print abcaaadefbbbghiccc | scan '(.)\1\1' | each ( | chomp | x 3 | pomch )
    aaa
    bbb
    ccc
    > print ABCDEFGHIJKABCDEFGHIJK  | scan -ignore-case a
    A
    A
    > split -target "AAA BBB CCC DDD EEE" | scan -multi-line "BBB\nCCC"
    BBB
    CCC
    > print abcaaadefbbbghiccc | scan "(.)\1\1" ( MATCH )
    aaa
    bbb
    ccc
    > print abcaaadefbbbghiccc | scan '(.)\1\1' ( 1|chomp; 1|chomp; 1 )
    aaa
    bbb
    ccc
    > print abcaaadefbbbghiccc | scan '(.)\1\1' ( PREMATCH )
    abc
    def
    ghi
    > print abcaaadefbbbghiccc | scan '(.)\1\1' ( POSTMATCH )
    defbbbghiccc
    ghiccc


strip
-
It is a filter for removing front and tail spaces, tabs, and line fields.

    > print \n\naaabbbccc\n\a | strip
    aaabbbccc
    > print " aaa bbb ccc ddd\n\n" | strip | split
    aaa
    bbb
    ccc
    ddd

lstrip
-
It is a filter for removing front spaces, tabs, and line fields.

    > print "\n\naaa\nbbb\nccc\n\n" | lstrip
    aaa
    bbb
    ccc

rstrip
-
It is a filter for removing spaces, tabs, and line fields.

    > print "\n\naaa\nbbb\nccc\n\n" | rstrip

    aaa
    bbb
    ccc

substr (index) (length)
-
This is filter which gets the part of input.

    -byte assume text encode as byte code.
    -utf8 assume text encode as utf-8 code.
    -sjis assume text encode as SJIS code.
    -eucjp assume text encode as EUCJP code.

    > print abcdefg | substr 1
    bcdefg
    > print abcdefg | substr 1 2
    bc
    > print abcdefg | substr 1 -1
    bcdef
    > print abcdefg | substr -1 1
    g
    > print abcdefg | substr -2
    fg
    > print abcdefg | substr -2 1
    f

substr_replace (replace) (index) (length)
-
This is a filter which replace the part of input.

    -byte assume text encode as byte code.
    -utf8 assume text encode as utf-8 code.
    -sjis assume text encode as SJIS code.
    -eucjp assume text encode as EUCJP code.

    > print abcdefg | substr_replace AAA 1 3
    aAAAefg
    > print abcdefg | substr_repalce AAA 1 0
    aAAAbcdefg
    > print abcdefg | substr_replace AAA 1 -1
    aAAA
    > print abcdefg | substr_replace AAA -2
    abcdeAAA
    > print abcdefg | substr_replace AAAr -1 0
    abcdefAAAg

combine (block) (block) ... (block)
-
Combine each output of blocks.

    -Lw Processing with CRLF line field
    -Lm Processing with CR line field
    -Lu Processing with LF line field
    -La Processing with BEL line field

    > combine ( print aaa\nbbb\nccc\nddd\neee\n ) ( print AAA\nBBB\nCCC\n )
    aaa
    AAA
    bbb
    BBB
    ccc
    CCC
    ddd
    eee

tr (characters) (characters2)
-
Filter which replace characters. Format of characters is the same as tr(1).

abc --> abc
a-c --> abc
0-3 --> 0123
^a --> except a

    -byte assume text encode as byte code.
    -utf8 assume text encode as utf-8 code.
    -sjis assume text encode as SJIS code.
    -eucjp assume text encode as EUCJP code.

    > print abc | tr ab Z | pomch
    ZZc
    > print abc | tr a-z A-Z | pomch
    ABC
    > print abc | tr a-z B-ZA | pomch
    BCD
    > print abcbca | tr abc YKL | pomch
    YKLKLY
    > print abcdef | tr a-c ^a Z | pomch
    aZZdef

delete (characters)
-
Filter which deletes characters. Format of characters is the same as tr(1).

abc --> abc
a-c --> abc
0-3 --> 0123
^a --> except a

    -byte assume text encode as byte code.
    -utf8 assume text encode as utf-8 code.
    -sjis assume text encode as SJIS code.
    -eucjp assume text encode as EUCJP code.

    > print abcdefghi | delete a-z ^b-c | pomch
    bc
    > print abcdefghi | delete a-c | pomch
    defghi
    > print abcdefghi | delete ^a-c | pomch
    defghi
    > print あいうえお | delete -utf8 ^あ | pomch
    あ

    squeeze (characters)
    -
    Filter which put together successive characters. Format of characters is the same as tr(1).

    abc --> abc
    a-c --> abc
    0-3 --> 0123
    ^a --> except a

    If there is no argument, put together all successive characters.

    -byte assume text encode as byte code.
    -utf8 assume text encode as utf-8 code.
    -sjis assume text encode as SJIS code.
    -eucjp assume text encode as EUCJP code.

    > print aaabbbcccdddeeefff | squeeze a-c | pomch
    abcdddeeefff
    > print aaabbbcccdddeeefff | squeeze ^a-c | pomch
    aaabbbcccdef
    > print あああいいいうううえええおおお | squeeze -utf8 あいうえお | pomch
    あいうえお

count (characters)
-
Output the number of characters. Format of characters is the same as tr(1).

abc --> abc
a-c --> abc
0-3 --> 0123
^a --> except a

    -byte assume text encode as byte code.
    -utf8 assume text encode as utf-8 code.
    -sjis assume text encode as SJIS code.
    -eucjp assume text encode as EUCJP code.

    > print aaabbbcccdddeeeff | count a-c
    9
    > print あいうえおaaabbbcccあいうえお | count -utf8 あい
    4
    > print aaabbbcccdddeeefff | count a-c
    9

succ
-
Filter which output next string.

    > print abc | succ
    abd
    > print main001 | succ | succ
    main003
    > print main0.0.1 | succ | succ
    main0.0.3
    > print 0.9.9 | succ
    1.0.0

split (regex)
-
It is a filter for spliting pipe data with regex.
If you omit the argument, xyzsh set "\s+" for it.

    -target (str) run filter with argument string.
    -no-regex Don't use regex for pattern. Use it as text.
    -ignore-case ignore case
    -multi-line Allow to write multiline matching regex, but the performance is less than normal.
    -byte assume text encode as byte code.
    -utf8 assume text encode as utf-8 code.
    -sjis assume text encode as SJIS code.
    -eucjp assume text encode as EUCJP code.
    -Lw Processing with CRLF line field
    -Lm Processing with CR line field
    -Lu Processing with LF line field
    -La Processing with BEL line field

    > split -target "aaa bbb ccc"
    aaa
    bbb
    ccc
    > print "aaa bbb ccc"  | split
    aaa
    bbb
    ccc
    > print "aaa,bbb,ccc" | split ,
    aaa
    bbb
    ccc
    > print "aaa.bbb.ccc" | split -no-regex .
    aaa
    bbb
    ccc

add (string)
-
It is a filter for adding string to pipe data.

    -index (number) add the string at the number position.
    -number (number) add the string at the number position.
    -byte assume text encode as byte code.
    -utf8 assume text encode as utf-8 code.
    -sjis assume text encode as SJIS code.
    -eucjp assume text encode as EUCJP code.

    > print aaa | add X | add X | add X | pomch
    aaaXXX
    > print abcdefghi | add -index 1 XXX | pomch
    aXXXbcdefghi
del (index)
-
It is a filter for removing a character on pipe data.

    -number (number) Number characters are removed.
    -byte assume text encode as byte code.
    -utf8 assume text encode as utf-8 code.
    -sjis assume text encode as SJIS code.
    -eucjp assume text encode as EUCJP code.

    > print 0123456789 | del 1 | pomch
    023456789
    > print 0123456789 | del 1 -number 2 | pomch
    03456789

rows (number1) (block) (number2) (block), ... , (number X) (block X)
-
Run block with each indicated character number. Context pipe in block has the characters.
You can use range for character number
(character number1)..(character number2)
Character number begins from 0. <0 is counted from tail.
A case of (character number1) > (character number2), reverse the order.

-byte assume text encode as byte code.
    -utf8 assume text encode as utf-8 code.
    -sjis assume text encode as SJIS code.
    -eucjp assume text encode as EUCJP code.

    > print あいうえおかきくけこ | rows -utf8 0 0 0 | pomch
    あああ
    > print あいうえおかきくけこ | rows -utf8 1..2 | pomch
    いう
    > print あいうえおかきくけこ | rows -utf8 -1..0 | pomch
    こけくきかおえういあ
    > print あいうえおかきくけこ | rows -utf8 -1 -1 -1 | pomch
    こここ
    > print あいうえおかきくけこ | rows -utf8 1200 -100 -100 | pomch

    > print abcdefghijk | rows 0 ( | uc ) 1..-1 ( | pomch )
    Abcdefghijk

readline (prompt string) | readline (prompt string) (block)
-
Using readline, get a line input by the user, and output it.

    "readline" innser command can do completion with output of block.

    -no-completion Using readline with no completion

    > readline -no-completion "Select yes or no > " |=~ ^y && print "selected yes"
    Select yes or no > yes
    selected yes

    > readline "type command line > " | eval
    type command line > pwd
    /Users/ab25cq

    > readline "Select yes or no > " ( split -target "yes no" ) |=~ ^y && print "selected yes"
    Select yes or no > [TAB]
    yes no


jump
-
Select menu for changing directory. A user function defined in xyzsh.xyzsh。The jump menu definition is written in ~/.xyzsh/jump.

    > cat ~/.xyzsh/jump
    /etc/
    /var/log/

    > jump
    /etc/
    /var/log

menu
-
Select menu for running a one line command. A user function defined in xyzsh.xyzsh. The menu definition is written in ~/.xyzsh/menu.

    > cat ~/.xyzsh/menu
    pwd
    ls
    whoami
    > menu

fselector
-
selecting file and output it.

    up key or C-p --> up cursor
    down key or C-n --> down cursor
    left key or C-b --> cursor left
    right key or C-f --> cursor right
    C-l --> refresh screen
    \ --> move to root directory
    C-h or Backcpace --> move to parent directory

    TAB key or 'w' --> determined the selected file
    q or C-c or Escape C-g --> cancel

    -multiple allow to select multiple files with typing SPACE key

point
-
output cursor position.

point_move (number)
-
cursor move on completion.

initscr
-
Start to curses mode.

endwin
-
Finish to curses mode.

getch
-
Output inputed key number

move (Y Position) (X Position)
-
Move cursor position.

refresh
-
Write off-screen content to screen.

clear
-
Clear screen. Don't write to screen until "refresh" inner command.

printw (format string)
-
Write (format string) to off-screen. "printw" inner command uses pipe content to format argument.
    -Lw Processing with CRLF line field
    -Lm Processing with CR line field
    -Lu Processing with LF line field
    -La Processing with BEL line field

is_raw_mode
-
If curses mode is running, return the return code of true.

