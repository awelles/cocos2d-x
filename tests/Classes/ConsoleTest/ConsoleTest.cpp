/****************************************************************************
 Copyright (c) 2013 cocos2d-x.org

 http://www.cocos2d-x.org

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#include "ConsoleTest.h"
#include "../testResource.h"
#include <stdio.h>
#include <stdlib.h>
#if (CC_TARGET_PLATFORM != CC_PLATFORM_WIN32)
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#else
#include <io.h>
#include <WS2tcpip.h>
#endif

//------------------------------------------------------------------
//
// ConsoleTest
//
//------------------------------------------------------------------

static int sceneIdx = -1;

static std::function<Layer*()> createFunctions[] =
{
    CL(ConsoleCustomCommand),
    CL(ConsoleUploadFile),
};

#define MAX_LAYER    (sizeof(createFunctions) / sizeof(createFunctions[0]))

Layer* nextConsoleTest()
{
    sceneIdx++;
    sceneIdx = sceneIdx % MAX_LAYER;

    auto layer = (createFunctions[sceneIdx])();
    return layer;
}

Layer* backConsoleTest()
{
    sceneIdx--;
    int total = MAX_LAYER;
    if( sceneIdx < 0 )
        sceneIdx += total;

    auto layer = (createFunctions[sceneIdx])();
    return layer;
}

Layer* restartConsoleTest()
{
    auto layer = (createFunctions[sceneIdx])();
    return layer;
} 


BaseTestConsole::BaseTestConsole()
{
}

BaseTestConsole::~BaseTestConsole(void)
{
}

std::string BaseTestConsole::title() const
{
    return "No title";
}

void BaseTestConsole::onEnter()
{
    BaseTest::onEnter();
}

void BaseTestConsole::restartCallback(Ref* sender)
{
    auto s = new ConsoleTestScene();
    s->addChild(restartConsoleTest());

    Director::getInstance()->replaceScene(s);
    s->release();
}

void BaseTestConsole::nextCallback(Ref* sender)
{
    auto s = new ConsoleTestScene();
    s->addChild( nextConsoleTest() );
    Director::getInstance()->replaceScene(s);
    s->release();
}

void BaseTestConsole::backCallback(Ref* sender)
{
    auto s = new ConsoleTestScene();
    s->addChild( backConsoleTest() );
    Director::getInstance()->replaceScene(s);
    s->release();
} 

void ConsoleTestScene::runThisTest()
{
    auto layer = nextConsoleTest();
    addChild(layer);

    Director::getInstance()->replaceScene(this);
}


//------------------------------------------------------------------
//
// ConsoleCustomCommand
//
//------------------------------------------------------------------

ConsoleCustomCommand::ConsoleCustomCommand()
{
    _console = Director::getInstance()->getConsole();
    static struct Console::Command commands[] = {
        {"hello", "This is just a user generated command", [](int fd, const std::string& args) {
            const char msg[] = "how are you?\nArguments passed: ";
            send(fd, msg, sizeof(msg),0);
            send(fd, args.c_str(), args.length(),0);
            send(fd, "\n",1,0);
        }},
    };
    _console->addCommand(commands[0]);
}

ConsoleCustomCommand::~ConsoleCustomCommand()
{
}

void ConsoleCustomCommand::onEnter()
{
    BaseTestConsole::onEnter();
}

std::string ConsoleCustomCommand::title() const
{
    return "Console Custom Commands";
}

std::string ConsoleCustomCommand::subtitle() const
{
    return "telnet localhost 5678";
}


//------------------------------------------------------------------
//
// ConsoleUploadFile
//
//------------------------------------------------------------------

ConsoleUploadFile::ConsoleUploadFile()
{
    srand (time(NULL));
    int _id = rand()%100000;
    char buf[32];
    sprintf(buf, "%d", _id);
    _target_file_name = std::string("grossini") + buf;

   _src_file_path = FileUtils::getInstance()->fullPathForFilename(s_pathGrossini);
    _thread = std::thread( &ConsoleUploadFile::uploadFile, this);
}

void ConsoleUploadFile::onEnter()
{
    BaseTestConsole::onEnter();
    
}

ConsoleUploadFile::~ConsoleUploadFile()
{
    _thread.join();
}
void ConsoleUploadFile::uploadFile()
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sfd, s, j;

    /* Obtain address(es) matching host/port */

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* stream socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          /* Any protocol */

#if (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2),&wsaData);
#endif

    s = getaddrinfo("localhost", "5678", &hints, &result);
    if (s != 0) 
    {
       CCLOG("ConsoleUploadFile: getaddrinfo error");
        return;
    }

    /* getaddrinfo() returns a list of address structures.
      Try each address until we successfully connect(2).
      If socket(2) (or connect(2)) fails, we (close the socket
      and) try the next address. */

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype,
                    rp->ai_protocol);
        if (sfd == -1)
            continue;

        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break;                  /* Success */

#if (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
        closesocket(sfd);
#else
        close(sfd);
#endif
    }

    if (rp == NULL) {               /* No address succeeded */
        CCLOG("ConsoleUploadFile: could not connect!");
        return;
    }

    freeaddrinfo(result);           /* No longer needed */

    
    FILE* fp = fopen(_src_file_path.c_str(), "rb");
    if(!fp)
    {
        CCLOG("ConsoleUploadFile: could not open file %s", _src_file_path.c_str());
        return;
    }

    //read file size
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char sb[32];
    sprintf(sb, "%d", size);
    std::string tmp = "upload";

    tmp += " ";
    tmp += _target_file_name;
    tmp += " ";
    tmp += sb;
    tmp += "\n";
    char cmd[512];

    strcpy(cmd, tmp.c_str());
    send(sfd,cmd,strlen(cmd),0);

    // allocate memory to contain the whole file:
    char* buffer = (char*) malloc (sizeof(char)*size);
    if (buffer == NULL) 
    {
        CCLOG("ConsoleUploadFile: memory allocate error!");
        return;
    }

    // copy the file into the buffer:
    int ret = fread(buffer, 1, size, fp);
    if (ret != size)
    {
        CCLOG("ConsoleUploadFile: read file: %s error!",_src_file_path.c_str());
        return;
    }

    //send to console socket
    for(int i = 0; i < size; i++)
    {
        send(sfd, &buffer[i], 1, 0);
    }

    // terminate
    fclose (fp);
    free (buffer);
    
#if (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
        closesocket(sfd);
        WSACleanup();
#else
        close(sfd);
#endif
    return;
}

std::string ConsoleUploadFile::title() const
{
    return "Console UploadFile";
}

std::string ConsoleUploadFile::subtitle() const
{
    auto sharedFileUtils = FileUtils::getInstance();
    
    std::string writablePath = sharedFileUtils->getWritablePath();

    return "file uploaded to:" + writablePath + _target_file_name;
}

