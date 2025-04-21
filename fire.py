#!/usr/bin/python

#import time
import datetime
import tzlocal
import firebase_admin
from firebase_admin import credentials
from firebase_admin import db
from datetime import datetime
import os
import json

# open up the firebase using certificates in credentials.json
# this file is generated in firebase.console.com
def open_firebase(SIMPLE_PATH):
    #SIMPLE_PATH = os.getcwd ( )
    print (SIMPLE_PATH)
    with open ( SIMPLE_PATH + '/credentials.json' ) as f :
          data = json.load ( f )
          project_https = 'https://' + data['project_id'] + '.firebaseio.com/'
          con_string = '{"databaseURL":"' + project_https + '"}'
          js = json.loads ( con_string )
          print js['databaseURL']
          # print project_https

    cred = credentials.Certificate ( SIMPLE_PATH + "/credentials.json" )
    firebase_admin.initialize_app ( cred, js )

    data = {".sv" : "timestamp"}
    # timestamp = firebase.put('/temp', '/temp', data)
    root = db.reference ( '/temp' )
    root.set ( data )
    timestamp = db.reference ( '/temp' ).get ( )
    print timestamp

# get UTC datetime stamp from firebase, convert to local time according time zone
def firebase_stamp():
      __fire_time = 0
      data = {".sv": "timestamp"}
      #timestamp = firebase.put('/temp', '/temp', data)
      root = db.reference('/temp') 
      root.set(data)
      timestamp = db.reference('/temp').get()
      local_timezone = tzlocal.get_localzone()
      local_time = datetime.fromtimestamp(timestamp / 1000, local_timezone)
      __fire_time = timestamp / 1000  # local_time.time()
      print "Firebird: ",timestamp,local_time.strftime("%Y-%m-%d %H:%M:%S")
      # print "Fire: ",self.__fire_time
      return __fire_time 

def push_data(t):
      root = db.reference ( '/' )
      result = root.child ( 'repx' ).push ( t )
      print "data has been sent"
