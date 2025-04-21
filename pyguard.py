#!/usr/bin/python
import sys
import threading
import os
import signal
#import firebase
#import firebase_admin
#from firebase_admin import credentials
#from firebase_admin import db

from subprocess import check_output, CalledProcessError
import socket
import uuid
import time
import datetime
import tzlocal
import json
#import fire


from datetime import datetime

# 3rd party modules
import sysv_ipc

MESSAGE_PIPE = 44
MSG_TO_CPP = 45

# po tomhle intervalu prohlasim FB za nedostupnou
FIREBASE_DEAD = 10
SIMPLE_DEAD = 60
DEBUG = False

# 1.. proprietary server
# 2.. FIREBASE
SERVER_TYPE = 1
SIMPLE_PATH=os.path.dirname(os.path.abspath(__file__))

def kill_process(process):
    try:
        pidlist = map(int, check_output(["pidof", process]).split())
    except  CalledProcessError:
        pidlist = []
    # print ' list of PIDs = ' + ', '.join(str(e) for e in pidlist)
    for e in pidlist:
        print ("kill ", e);
        os.kill(e, signal.SIGKILL)


def exists_process(process):
    try:
        pidlist = map(int, check_output(["pidof", process]).split())
    except CalledProcessError:
        pidlist = []

    for e in pidlist:
        return 1
    return 0


# trying find the process against its name, kill it and restart again
def restart_process(process):
    try:
        pidlist = map(int, check_output(["pidof", process]).split())
    except  CalledProcessError:
        pidlist = []
    # print ' list of PIDs = ' + ', '.join(str(e) for e in pidlist)
    for e in pidlist:
        print (e);
        os.kill(e, signal.SIGKILL)
    cmd = "cd %s; rmmod ftdi_sio ; ./%s &" % (SIMPLE_PATH, process)
    ret = os.system(cmd)
    print ("restarted ", ret)
    time.sleep(1)  # give the "./simple" process time to connect to the server
    return ret


IsMessage = 0
MessageQueue = ""


def get_network_ip(IpAdress):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.connect((IpAdress, 0))
    return s.getsockname()[0]


def getval(parname):
    with open(SIMPLE_PATH+"/lan_reader_cfg.txt") as f:
        for line in f:
            if parname in line:
                ret = (line.split("=", 1)[1])
                ret = ret.replace('\r', '')
                ret = ret.replace('\n', '')
                return (ret)


def get_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        # doesn't even have to be reachable
        s.connect(('10.255.255.255', 1))
        IP = s.getsockname()[0]
    except:
        IP = '127.0.0.1'
    finally:
        s.close()
    return IP
    # print "the socket has successfully connected to google on port == %s" % (host_ip)


def fmt_stamp(TimeStamp):
    local_timezone = tzlocal.get_localzone()
    local_time = datetime.fromtimestamp(TimeStamp, local_timezone)
    ret = local_time.strftime("%Y-%m-%d %H:%M:%S")
    return ret

# event types
class Enum(set):
    def __getattr__(self, name):
        if name in self:
            return name
        raise AttributeError


EventType = Enum(["BTN", "ATV", "KEY"])


class event:
    checkpoint = "2D6CAC"
    timestamp = 1580311413  # type: Any
    etype = EventType.BTN
    info = 0


# message queue is read in this thread
class PrimeNumber(threading.Thread):
    lock = threading.Lock()

    def __init__(self, number):
        threading.Thread.__init__(self)
        self.__server_type = number
        self.__sok_time = 0
        self.__any_time = 0
        self.__fire_time = 0
        self.__was_stdout = False
        self.__json = ""
        self.__isjson = False
        self.__autoUser = int(getval("_auto_user"))
        self.__userName = getval("_user")
        self.__serverName = getval("_host")
        self.__serverPort = int(getval("_port"))
        self.__mac = ':'.join(['{:02x}'.format((uuid.getnode() >> i) & 0xff) for i in range(0, 8 * 6, 8)][::-1])
        self.__mylocalip = get_ip()
        self.__ascii = ''
        self.__isAdapterOK = True;
    @property

    def run(self):
        global IsMessage
        global MessageQueue
        global firebase
        global db
        global diff

        def  chk_and_ack():
            try:
                ret = False
                s = socket.socket ( socket.AF_INET, socket.SOCK_STREAM )

                s.settimeout(1.0)
                s.connect((self.__serverName, self.__serverPort))
                mac = uuid.getnode()
                stim = "CHK %2x %s\r\n" % (mac, self.__userName)
                #print (stim,"xx")
                #exit
                s.sendall (stim.encode())
                time.sleep ( 0.2 )
                reply = s.recv ( 32 ).decode()
                if reply.find("ACK") > -1:
                    ret = True

            except:
                ret = False
            finally:
                s.close()
                return(ret)

        def _linux_set_time(time_tuple) :
            import ctypes
            import ctypes.util
            import time
            CLOCK_REALTIME = 0

            class timespec( ctypes.Structure ) :
                _fields_ = [("tv_sec", ctypes.c_long),
                            ("tv_nsec", ctypes.c_long)]

            librt = ctypes.CDLL ( ctypes.util.find_library ( "rt" ) )
            ts = timespec ( )
            ts.tv_sec = int ( time.mktime ( datetime ( *time_tuple[:6] ).timetuple ( ) ) )
            ts.tv_nsec = time_tuple[6] * 1000000  # Millisecond to nanosecond
            # http://linux.die.net/man/3/clock_settime
            librt.clock_settime ( CLOCK_REALTIME, ctypes.byref ( ts ) )

        def save_json(jload):
            file_name = "%s/data/data_%s.json" % (SIMPLE_PATH,time.strftime("%Y_%m_%d__%H_%M_%S"))
            file = open(file_name,"w")
            file.write(jload)
            file.close()

        def json_to_prop(jload):            #convert json to proprietary format
            #fo = open("ascii.txt", "wb")
            #fo.write(jload)
            try:
                ret = True
                s = socket.socket ( socket.AF_INET, socket.SOCK_STREAM )
            except socket.error as err:
                print ("socket creation failed with error %s" % (err))
                return (False)

            try :
                # prepare stimulus
                ret = True
                s.settimeout (2.0)
                s.connect((self.__serverName, self.__serverPort))

                mac = uuid.getnode ( )
                lin = "CHK %2x %s\r\n" % (mac, self.__userName)
                print (lin,"x")
                #exit

                #00-000024241207*0E
                self.__userName =jload["adapter"].replace("00-0000", "").replace("*", "")

                lin = "CHK %2x %s\r\n" % (mac,self.__userName)
                s.sendall(lin.encode())
                reply=s.recv(6).decode()

                #stim = "GST %s\r\n" % (time.strftime ( "%Y-%m-%d %H:%M:%S" ))
                data = jload["data"]
                lin = "USR %s %2x\r\n" %(self.__userName, mac)
                s.sendall (lin.encode())
                time.sleep ( 0.1 )
                reply = s.recv(6).decode()

                lin = "PES %s %s \r\n" % (fmt_stamp(jload["timestamp"]), jload["pes"])
                #s.sendall (lin)
                s.send(lin.encode())
                #time.sleep ( 0.5 )
                #reply = s.recv (6)

                lin ="IDS %s %s\r\n" % (jload["idsyn"],mac)
                s.send(lin.encode())

                for tuple in data :
                    for ele in tuple :
                        # print ele, tuple[ele]
                        if ele == "I" :
                            event.etype = EventType.BTN
                            event.checkpoint = tuple[ele]
                        elif ele == "A":
                            event.etype = EventType.ATV
                            event.checkpoint = tuple[ele]
                        elif ele == "F":
                            event.etype = EventType.ATV
                            event.info = tuple[ele]
                        elif ele == "K":
                            event.etype = EventType.KEY
                            event.checkpoint = tuple[ele]
                        elif ele == "T" :
                            event.timestamp = tuple[ele]
                    #lin = event.etype, event.timestamp, fmt_stamp ( event.timestamp ), event.checkpoint

                    lin = "%s %s %s \r\n" %(event.etype,fmt_stamp(event.timestamp), event.checkpoint)
                    if event.etype  == EventType.ATV:
                        lin ="%s %s %s %d\r\n" %(event.etype, fmt_stamp(event.timestamp), event.checkpoint, event.info)
                    print (lin)

                    s.sendall (lin.encode())
                    time.sleep ( 0.01 )
                    #reply = s.recv ( 6 ).decode()

                lin = "END \r\n"
                s.sendall (lin.encode())
                time.sleep ( 0.1 )
                reply = s.recv(6).decode()

            except socket.error as err:
                 print ("error %s" % (err))
                 return (False)
            finally:
                s.close()
            #print_socket(line,s)

        def send_and_return(stimulus):
            # return missing data
            try:
                ret = True
                s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            except socket.error as err:
                print ("socket creation failed with error %s" % (err))
                return False

            try:
                # prepare stimulus
                ret = True
                s.settimeout(0.1)
                s.connect((self.__serverName, self.__serverPort))
                #print ("send_and_return po s.connect\r\n")
                tim = time.time()
                print (repr(stimulus))
                x = stimulus.encode()
                s.send(x)
                time.sleep(2.5)
                reply = s.recv(33).decode()
                #print ("odpoved ze send_and_return")
                #print reply
                return reply

            except socket.timeout as e:
                # except err:
                print ("send_and_return timeout\r\n")
                return False
            except:
                return False
                print ("Unexpected error in send_and_return: ",sys.exc_info()[0])
            finally:
                s.close()
            return True


        def prop_server_stamp():
            try:
                ret = True
                s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            except socket.error as err:
                # print "socket creation failed with error %s" % (err)
                return False
            try:
                # prepare stimulus
                ret = True
                s.settimeout(0.1)
                s.connect((self.__serverName, self.__serverPort))
                tim = time.time()
                stimulus = "GST %s\r\n" % (time.strftime("%Y-%m-%d %H:%M:%S +0000"))
                s.sendall(stimulus.encode())
                time.sleep(0.2)
                reply = s.recv(32).decode()
                stimulus = stimulus.replace("\r\n", "")
                # SST 2020-02-09 09:00:50
                if reply.find("SST") > -1:
                    # get only datetime part of string
                    spl = reply.split()
                    datetime_str = spl[1] + ' ' + spl[2]
                    svctime = time.mktime(time.strptime(datetime_str, "%Y-%m-%d %H:%M:%S"))
                    print (svctime - time.time())
                    self.__fire_time = svctime
                    s.close()
                    return True

                if reply.find("NCK") > -1:
                    return False

                if reply.find("ACK") > -1:
                    self.__fire_time = time.time()
                    return True

            except socket.timeout as e:
                # except err:
                return False

            except:
                return False
                print ("Unexpected error: ",sys.exc_info()[0])
            finally:
                s.close()
            return True

        def server_stamp():
            if SERVER_TYPE == 1:
                ret = prop_server_stamp()
            elif SERVER_TYPE == 2:
                self.__fire_time = fire.firebase_stamp()
                ret = self.__fire_time>0
            else:
                return (False)
            return (ret)

        def read_json():
            final = False
            i = 0
            s = ""
            self.__json = ""
            while not final:
                mm = mq.current_messages  # how many events we have in message_queue
                if (mm > 0):
                    message_queue = mq.receive()
                    s = message_queue[0]
                    s = s.decode()
                    print (s)

                    if s.find("EJS") > -1:
                        final = True
                        mw.send("EJK")
                        print ("---- final read_json")
                        # time.sleep(0.5)
                    else:
                        mw.send("OJL")
                        self.__json = self.__json + s
                    # final = i>30
                    # i = i+1
            # time.sleep(0.1)
            self.__isjson = final
            print ("xxxx", i)

        def json_validator(data):
            try:
                json.loads(data)
                return True
            except ValueError as error:
                print("invalid json: %s" % error)
            return False

        def clear_pipe(shmem):
            try:
               count =  shmem.current_messages
               while count > 0:
                   msqueue = shmem.receive()
                   print (msqueue[0])
                   count = shmem.current_messages

            except:
               print ("chyba clear_pipe")

        def calc_diff():
            loc = time.time()
            return loc - self.__fire_time

        diff = 0
        clear_pipe(mq)
        clear_pipe(mw)
        while True:
            try:
                # in this section we are processing queue from ./simple process
                # ./simple always initiates conversation
                mm = mq.current_messages
                if mm > 0:
                    PrimeNumber.lock.acquire()
                    self.__any_time = time.time()
                    MessageQueue = mq.receive()
                    s = MessageQueue[0].decode()

                    print ("xPC: ", mm, time.strftime("%Y-%m-%d %H:%M:%S"), MessageQueue, s)
                    # s contains request from ./simple process
                    # rest of code just serves this request
                    if s.find("SJS") > -1:
                        mw.send("SJO")  # acknowledge packet
                        time.sleep(0.1)
                        self.__isjson = True
                        read_json()
                        self.__isjson = False
                        t = json.loads(self.__json)  # parser
                        if SERVER_TYPE == 1:
                            # The server is proprietary -> data will be converted from JSON to plain text
                            # conversation with server is very rude, you sent line ended #13#10
                            # server acknowledges packet with ACK#13#10
                            # for example:
                            # CHK
                            # PES 2017-02-08 15:30:10 31259145
                            # BTN 2017-02-08 15:30:07 F75D86
                            # BTN 2017-02-08 15:30:07 F75D86
                            # END
                            # CHK
                            result = json_to_prop(t)  # save data to file
                        elif SERVER_TYPE == 2:
                            # Target is FIREBASE, just push json into
                            #root = db.reference('/')
                            #result = root.child('repx').push(t)
                            result = fire.push_data ( t )

                        save_json(self.__json)
                        if not result:
                            self.__fire_time=0
                        print (result)


                    # have we seen database one minute before ?
                    if s.find("CFF") > -1:
                        #print "CFF prijato"
                        clear_pipe(mw)
                        self.__sok_time = time.time()
                        loc = time.time()
                        diff = loc - self.__fire_time
                        if diff < FIREBASE_DEAD:
                            # OFB ... OK FIREBASE
                            mw.send("OFB")
                            print ("CFF->OFB")
                        else:
                            mw.send("EFB")
                            print ("CFF->EFB")

                    # automatic user name from adapter number
                    if s.find("ADA")>-1:
                        clear_pipe(mw)
                        if self.__autoUser >0:
                            sp = s.split()
                            if len(sp)>1:
                                self.__userName = sp[1]
                                mw.send("AOK")
                                print ("ADA->AOK")
                            else:
                                mw.send("AER")

                    # ADS .. ADapter State
                    # dam serveru info o stavu adapteru
                    # python parametr za ADS jenom preposle na server
                    # reakce je stejna -> restart simple procesu
                    # ADS POWER_OFF -> chyba powerOFF
                    # ADS POWER_ON  -> chyba powerON
                    if s.find("ADS") > -1:
                        sp = s.split()
                        if len(sp)>1:
                            if (sp[1] != "OK"):
                                self.__isAdapterOK = False # chci restart
                            # sp[1] .. slovni popis chyby (POWER_OFF), sp[2] .. cislo chyby
                            stim  = self.__mac.replace(":","")
                            stim = "ADS {} {} {}\r\n".format(stim,sp[1],sp[2])
                            clear_pipe(mw)
                            stimulus = send_and_return(stim)
                            if (stimulus == False):
                                print(stim, " neprisla odpoved v ocekavanem case\r\n")
                                mw.send("ADS EFB")
                            else:
                                print (stimulus)
                                mw.send(stimulus)

                    # have we seen database one minute before ?
                    if s.find ("RMD") > -1:
                        clear_pipe(mw)
                        #print ("RMD po clear_pipe\r\n")
                        self.__mac = ''.join(['{:02x}'.format((uuid.getnode() >> i) & 0xff) for i in range(0, 8 * 6, 8)][::-1])
                        sp = s.split()
                        #print (sp,len(sp))
                        if len(sp) > 1:
                            self.__userName = sp[1]
                            stim = "RMD {} {}\r\n".format(self.__userName, self.__mac)
                            stimulus = send_and_return(stim)
                            if (stimulus == False):
                                print (stim," neprisla odpoved v ocekavanem case\r\n")

                            if (stimulus != False):
                                print(stimulus)
                                #clear_pipe(mw)
                                mw.send(stimulus) # odesli do c-eckoveho programu zpravu, ktera se vratila na RMD
                                print ("RMD po mw.send\r\n")

                    # Explicit request for server time.
                    if s.find("CFB") > -1:
                        self.__fire_time = 0
                        if server_stamp():
                            local_timezone = tzlocal.get_localzone()
                            local_time = datetime.fromtimestamp(self.__fire_time, local_timezone)
                            stimulus = "SST %s\r\n" % (local_time.strftime("%Y-%m-%d %H:%M:%S"))
                            print ("CFB  -> %s" % stimulus)
                        else:
                            stimulus = "EFB"
                        clear_pipe(mw)
                        mw.send(stimulus)

                    PrimeNumber.lock.release()

                # last sign of life on server
                loc = time.time()
                diff = loc - self.__fire_time
                if self.__fire_time > 0:
                    local_timezone = tzlocal.get_localzone()
                    local_time = datetime.fromtimestamp(self.__fire_time,local_timezone)
                    stimulus = "STT %s\r\n" % (local_time.strftime("%Y-%m-%d %H:%M:%S"))
                    #print "finally %d diff:%d(sec) :%s" % (mm, diff,stimulus)
                #else:
                    #print ("finally %d diff:%d/sec" % (mm, diff))

                if diff >= FIREBASE_DEAD:
                    if server_stamp() == True:
                        if abs(calc_diff()) > 3:
                            time_tuple = time.localtime(self.__fire_time)
                            _linux_set_time(time_tuple)
                            print ("Time was set",time_tuple)

                    # get server know we are alive
                    if SERVER_TYPE == 1:
                      chk_and_ack()

                # last time i've heard about ./simple process?
                diff = time.time()-self.__sok_time
                if (diff > SIMPLE_DEAD) or (self.__isAdapterOK == False):
                    #if exists_process("simple") > 0:
                    self.__isAdapterOK = True  # predpoklada, ze adapter se restartem napravi
                    self.__sok_time = time.time()
                    print ("Trying to restart ./simple")
                    if DEBUG == False:
                      if restart_process("simple") > 0:
                        ret = exists_process("simple")
                        print ("process_exists :%d", ret)


            except:
                print ("Unexpected error:", sys.exc_info()[0], mm, MessageQueue)
                exit(0)

            if mm < 1:
                time.sleep(0.1)


################################### MAIN ##########################
print ("xxxxx", get_ip())
if __name__ == '__main__':
    #SIMPLE_PATH = os.getcwd ()
    print ("os.getcwd() ",SIMPLE_PATH)
    if SERVER_TYPE == 2:
        fire.open_firebase(SIMPLE_PATH)

print (SIMPLE_PATH)
if DEBUG==False:
  print ("kill process simple")
  kill_process('simple')
else:
  print("***********************************************")
  print("* DEBUG DEBUG DEBUG DEBUG DEBUG DEBUG == True *")
  print("***********************************************")

if SERVER_TYPE==1:
    print ("................Server is proprietary")
else:
    print ("................Server is FIREBASE !!!")

# Create the message queue.
try:
    mq = sysv_ipc.MessageQueue(MESSAGE_PIPE, sysv_ipc.IPC_CREAT)
    mw = sysv_ipc.MessageQueue(MSG_TO_CPP, sysv_ipc.IPC_CREAT)
    thread = PrimeNumber(SERVER_TYPE)
    thread.start()
except sysv_ipc.ExistentialError:
    print ("ERROR: message queue creation failed")

if DEBUG==False:
  print ("restart simple")
  restart_process("simple")

startTime = time.time()
sokTime = time.time()
sok_restart = False

while True:
    try:
        print("main routine\r\n")
        time.sleep(1000)

    except:
        print ("Error\r\n")

mq.remove()
mw.remove()
print ("end")
