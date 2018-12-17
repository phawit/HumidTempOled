#!/usr/local/bin/python
# -*- coding: utf-8 -*-
# encoding=utf8  
import sys  

reload(sys)  
sys.setdefaultencoding('utf8')

import requests
import pyrebase
import datetime
import time

config = {
  "apiKey": "AIzaSyA8hvIgXiCLMJt0cLKtQI-mwge4WvQbIuo",
  "authDomain": "humidtemp-59706.firebaseapp.com",
  "databaseURL": "https://humidtemp-59706.firebaseio.com",
  "storageBucket": "humidtemp-59706.appspot.com",
  }

firebase = pyrebase.initialize_app(config)
db = firebase.database()

once = 0
xMin = int(datetime.datetime.now().strftime("%M"))

# unit_id = ['A01', 'A02', 'A03', 'A04']

# while(1):
  #check time
# current_time = int(datetime.datetime.now().strftime("%Y%m%d%H"))
Hr = int(datetime.datetime.now().strftime("%H"))
Min = int(datetime.datetime.now().strftime("%M"))


def lineNotify(userKey):
  DateTime = db.child("ID/"+user.key()+"/Current/DateTime").get().val()
  firebaseTime = int(DateTime[0:8]+DateTime[9:11])
  current_time = int(datetime.datetime.now().strftime("%Y%m%d%H"))

  # print(firebaseTime)

  # print(current_time)
  # print(current_time-firebaseTime)

  # if(current_time-firebaseTime > 2):   #check HT online in 2 hours
  if(current_time-firebaseTime < 2):
    print "HT not online"
  else:
      print("line notify")
 
      #line notify
      url = 'https://notify-api.line.me/api/notify'

      Flag = db.child("ID/"+userKey+"/Current/Flag").get().val()
      
      if(Flag=='Red'):
        Flag = '♦️'
      elif(Flag=='Yellow'):
        Flag = '🔶'
      elif(Flag=='Green'):
        Flag = '🇸🇦'
      elif(Flag=='Black'):
        Flag = '🏴'
      else:
        Flag = '🏳️'
        
      if(Hr>=6 and Hr <18):
        dayNight = "🔆"
      else: 
        dayNight = "🌜"

      UnitName = db.child("ID/"+user.key()+"/Current/UnitName").get().val()
      Temperature = db.child("ID/"+user.key()+"/Current/Temperature").get().val()
      Humid = db.child("ID/"+user.key()+"/Current/Humid").get().val()
      Water = db.child("ID/"+user.key()+"/Current/Water").get().val()
      Rest = db.child("ID/"+user.key()+"/Current/Rest").get().val()
      Train = db.child("ID/"+user.key()+"/Current/Train").get().val()
      
      # message =  UnitName+dayNight
      # message += "\n⛰Flag: "+flag[i]
      # message += "\n⛰Temperature: "+str(temp[i])+" °C"
      # message += "\n⛰Humid: "+str(humid[i])+" %"  
      # message += "\n⛰Water: "+str(water[i])+" L/hr"
      # message += "\n⛰Train: "+str(train[i])+" min"
      # message += "\n⛰Rest: "+str(rest[i])+" min"
      # message += "\nhttps://humidtemp-59706.firebaseapp.com/"
        
      messageTH =  UnitName+dayNight
      messageTH += "\n⛰สัญญาณธง: "+Flag
      messageTH += "\n⛰อุณหภูมิ: "+str(Temperature)+" °C"
      messageTH += "\n⛰ความซื้นสัมพัทธ์: "+str(Humid)+" %"  
      messageTH += "\n⛰ควรดื่มน้ำ: "+str(Water)+" ลิตร/ชม."
      messageTH += "\n⛰แนะนำให้ฝึก: "+str(Train)+" นาที"
      messageTH += "\n⛰พัก: "+str(Rest)+" นาที"
      messageTH += "\nhttps://humidtemp-59706.firebaseapp.com/"

      Line1 = db.child("ID/"+user.key()+"/Current/Line1").get().val()
      Line2 = db.child("ID/"+user.key()+"/Current/Line2").get().val()
      Line3 = db.child("ID/"+user.key()+"/Current/Line3").get().val()
      Line4 = db.child("ID/"+user.key()+"/Current/Line4").get().val()
          
      headers = {'content-type':'application/x-www-form-urlencoded','Authorization':'Bearer '+'xXmd9aMogNqYnuP1VIVuXZsbyuMBohpLcVUo3ukLR3N'}
      r = requests.post(url, headers=headers , data = {'message':messageTH})  
      headers = {'content-type':'application/x-www-form-urlencoded','Authorization':'Bearer '+Line1}
      r = requests.post(url, headers=headers , data = {'message':messageTH})
      headers = {'content-type':'application/x-www-form-urlencoded','Authorization':'Bearer '+Line2}
      r = requests.post(url, headers=headers , data = {'message':messageTH})
      headers = {'content-type':'application/x-www-form-urlencoded','Authorization':'Bearer '+Line3}
      r = requests.post(url, headers=headers , data = {'message':messageTH})
      headers = {'content-type':'application/x-www-form-urlencoded','Authorization':'Bearer '+Line4}
      r = requests.post(url, headers=headers , data = {'message':messageTH})


while(1):

  Hr = int(datetime.datetime.now().strftime("%H"))
  Min = int(datetime.datetime.now().strftime("%M"))

  print Hr
  print Min
  print " "
    
  if(abs(xMin-int(datetime.datetime.now().strftime("%M")))>1):
    once = 0
      
  if ((Hr == 8 or Hr == 10 or Hr == 13 or Hr == 15 or Hr == 17) and Min == 00)and once==0:
    print "time for Day notify"

    id = db.child("ID").get()
    for user in id.each():
      print(user.key()) # A01, A02, A03
      Alarm1 = db.child("ID/"+user.key()+"/Current/Alarm1").get().val()
      if Alarm1 == 1:
        print "Day notify"
        xMin = int(datetime.datetime.now().strftime("%M"))
        lineNotify(user.key())
        once = 1
        
  if (((Hr == 19 or Hr == 21) and Min == 00)  or (Hr == 5 and Min == 30))and once==0:
    print "time for Night notify"
    id = db.child("ID").get()
    for user in id.each():
      print(user.key()) # A01, A02, A03
      Alarm2 = db.child("ID/"+user.key()+"/Current/Alarm2").get().val()
      if Alarm2 == 1:
        print "Night notify"
        xMin = int(datetime.datetime.now().strftime("%M"))
        lineNotify(user.key())
        once = 1

  
  time.sleep(10)









# users = db.child("ID").get()
# # print(users.val())

# print(users.key())

# id = db.child("ID").get()
# for user in id.each():
#     print(user.key()) # Morty
#     # print(user.val()) # {name": "Mortimer 'Morty' Smith"}
    
#     UnitName = db.child("ID/"+user.key()+"/Current/UnitName").get().val()
    
#     Temperature = db.child("ID/"+user.key()+"/Current/Temperature").get().val()
#     Humid = db.child("ID/"+user.key()+"/Current/Humid").get().val()
#     Latitude = db.child("ID/"+user.key()+"/Current/Latitude").get().val()
#     Longtitude = db.child("ID/"+user.key()+"/Current/Longtitude").get().val()
#     Alarm1 = db.child("ID/"+user.key()+"/Current/Alarm1").get().val()
#     Alarm2 = db.child("ID/"+user.key()+"/Current/Alarm2").get().val()
#     Battery_level = db.child("ID/"+user.key()+"/Current/Battery_level").get().val()
#     DateTime = db.child("ID/"+user.key()+"/Current/DateTime").get().val()
#     Flag = db.child("ID/"+user.key()+"/Current/Flag").get().val()
#     Line1 = db.child("ID/"+user.key()+"/Current/Line1").get().val()
#     Line2 = db.child("ID/"+user.key()+"/Current/Line2").get().val()
#     Line3 = db.child("ID/"+user.key()+"/Current/Line3").get().val()
#     Line4 = db.child("ID/"+user.key()+"/Current/Line4").get().val()
#     Rest = db.child("ID/"+user.key()+"/Current/Rest").get().val()
#     Train = db.child("ID/"+user.key()+"/Current/Train").get().val()




