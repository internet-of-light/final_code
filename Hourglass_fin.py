#import urllib
#import os
#import logging
#import random
import time
import datetime
import json
import schedule # Schedule lighting changes / checks for daily palette
import paho.mqtt.client as mqtt # MQTT Client functions
import gspread  # reading from google sheets
from oauth2client.service_account import ServiceAccountCredentials

# Custom control functions - Philips_Control_Functions.py must be in the same directory as this file
from Philips_Control_Functions import changeLight, changeGroup

# Keep track of status of TTT and whether today has an Hourglass palette
TICTACTOE_RUNNING = False
TODAYS_PALETTE = "None"


#These are now defined in Philips_Control_Functions.py
# IP Address of Philips Hue "Sieg Master" Bridge
#sieg_master_ip = "172.28.219.179"
#sieg_master_ip = "10.0.1.16" #sam house

# API Token we have generated for Sieg Master Bridge
#sieg_master_token = "rARKEpLebwXuW01cNVvQbnDEkd2bd56Nj-hpTETB"
#sieg_master_token = "efzstVYGsi1LQDdNF2N4GoR4pSBjCPOpGOX-qK1e" #Sam house



# setup: Run once at the beginning
def setup():

    print("SETUP: Starting Setup")

    #reading google sheets
    print("\tSETUP: Setting up connection to Google Sheets...")
    scope = ['https://spreadsheets.google.com/feeds', 'https://www.googleapis.com/auth/drive']
    creds = ServiceAccountCredentials.from_json_keyfile_name('My Project-67dbdb16f10c.json', scope)
    #creds = ServiceAccountCredentials.from_json_keyfile_name('internet-of-light-b5fe2f45bb5b.json', scope)
    sheets_client = gspread.authorize(creds)
    global sheet
    sheet = sheets_client.open('IOL Palettes').sheet1
    print("\tSETUP: Sheets connected.")
    palettes_from_sheet = sheet.get_all_records()
    print("\tSETUP: Listing palettes from \"IOL Palettes\"")
    print("\tSETUP: Make sure everything you think should be there is!")
    for palette in palettes_from_sheet:
        if palette['PaletteName']:
            if palette['Date Active']:
                print("\t\t" + palette['PaletteName'] + " - " + palette['Date Active'])
            else:
                print("\t\t" + palette['PaletteName'])



    print("\tSETUP: Starting MQTT...")
    # MQTT Client Setup
    global mqtt_client
    mqtt_client = mqtt.Client(client_id="389ghcoeiuwhfjmd1943", clean_session=False)  # create new instance
    mqtt_client.on_connect = on_connect
    mqtt_client.reconnect_delay_set(min_delay=1, max_delay=120)
    #broker_address = "broker.mqttdashboard.com"
    #broker_address = "test.mosquitto.org"
    broker_address = "broker.hivemq.com"
    mqtt_connect_response = mqtt_client.connect(broker_address)
    print("\t\tSETUP: MQTT connection with response: " + str(mqtt_connect_response))
    mqtt_client.loop_start()
    mqtt_client.on_message = on_message
    #mqtt_client._keepalive = 60
    print("\tSETUP: MQTT Connected.")

    print("\tSETUP: Finding today's palette from Google Sheet")
    findDailyPalette()

    print("\tSETUP: Scheduling date-specific light updates:")
    print("\t\tSETUP: Execute findDailyPalette() once daily at 5am")
    schedule.every().day.at("05:00").do(findDailyPalette)
    #schedule.every(20).seconds.do(findDailyPalette) #testing
    print("\tSETUP: Tasks scheduled!")

    #changeLight(0,2,"hue","100")
    print("\nSETUP: Setup Complete!")



# loop: Run continuously
def loop():
    current_time = datetime.datetime.now()
    schedule.run_pending()
    #findDailyPalette()
    #print("Running!")
    #time.sleep(20)



# MQTT: The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
    print("MQTT ON CONNECT: Connected to MQTT server with result code "+str(rc))

    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    client.subscribe("hcdeiol")

# Return a readable version of json data for print debugging
def pretty(obj):  # help to read the json format easier to read
    return json.dumps(obj, sort_keys=True, indent=2)

# MQTT: Message Received Callback
# Checks if the message is JSON formatted and contains specific keys
def on_message(client, userdata, msg):
    topic=msg.topic
    m_decode=str(msg.payload.decode("utf-8","ignore"))
    print("MQTT ON_MESSAGE: MQTT Msg received: " +m_decode)
    m_in=json.loads(m_decode)

    #print(pretty(m_in))
    #print(m_in.keys())

    #Update hourglass's information on the status of the tic tac toe game
    if 'tictactoe' in m_in.keys():
        print("\tMQTT ON_MESSAGE: Found tictactoe in MQTT JSON message keys")
        tttStatus = m_in['tictactoe']
        print("\t\tMQTT ON_MESSAGE: Tic Tac Toe Status: " + tttStatus)
        global TICTACTOE_RUNNING
        if tttStatus == "on":
            print("\t\tMQTT ON_MESSAGE: Set TICTACTOE_RUNNING on")
            TICTACTOE_RUNNING = True
        elif tttStatus == "off":
            TICTACTOE_RUNNING = False
            print("\t\tMQTT ON_MESSAGE: Set TICTACTOE_RUNNING off")
    #Send a palette to the lights
    if 'Palette' in m_in.keys():
        print("\tMQTT ON_MESSAGE: Found palette in MQTT JSON message keys")
        palette_name = m_in['Palette']
        print("\t\tMQTT ON_MESSAGE: Palette switch: name = " + palette_name)
        fetchPaletteToLightsFromSheet(palette_name)

        # Send a palette to the lights
    if 'palette' in m_in.keys():
        print("\tMQTT ON_MESSAGE: Found palette in MQTT JSON message keys")
        palette_name = m_in['palette']
        print("\t\tMQTT ON_MESSAGE: Palette switch: name = " + palette_name)
        fetchPaletteToLightsFromSheet(palette_name)

    #Change an individual light
    if 'Lights' in m_in.keys():
        print("\tMQTT ON_MESSAGE: Found individual light in MQTT JSON message keys")
        for indiv_light_dict in m_in['Lights']:
            #print(indiv_light_dict.keys())
            for k in indiv_light_dict.keys():
                #print("\tMQTT ON_MESSAGE: Key: " + k)
                #print(indiv_light_dict[k][0].keys())
                print("\tMQTT ON_MESSAGE: Pushing change to light: " + str(k))
                on = indiv_light_dict[k][0]["on"]
                hue = indiv_light_dict[k][0]["hue"]
                sat = indiv_light_dict[k][0]["sat"]
                bri = indiv_light_dict[k][0]["bri"]
                changeLight(k, 2, "on", on, "hue", hue, "sat", sat, "bri", bri)

    #Change a group of lights
    if 'Groups' in m_in.keys():
        print("\tMQTT ON_MESSAGE: Found light group in MQTT JSON message keys")
        for group_light_dict in m_in['Groups']:
            print(group_light_dict.keys())
            for k in group_light_dict.keys():
                #print("Key: " + k)
                #print(group_light_dict[k][0].keys())
                on = group_light_dict[k][0]["on"]
                #print(on)
                print("\tMQTT ON_MESSAGE: Pushing full group change")
                if(on=="true"):
                    hue = group_light_dict[k][0]["hue"]
                    sat = group_light_dict[k][0]["sat"]
                    bri = group_light_dict[k][0]["bri"]
                    changeGroup(k, 2, "on", on, "hue", hue, "sat", sat, "bri", bri)
                else:
                    changeGroup(k, 2, "on", on)
                print("\tMQTT ON_MESSAGE: Pushed")


def findDailyPalette():

    current_date = datetime.datetime.now()

    yyyymmdd = current_date.strftime('%Y-%m-%d')

    print("Searching for palette matching date: " + yyyymmdd)

    palettes_from_sheet = sheet.get_all_records()

    for palette in palettes_from_sheet:
        if palette['Date Active'] == yyyymmdd:
            global TODAYS_PALETTE
            paletteName = palette['PaletteName']
            TODAYS_PALETTE = paletteName
            print("Found palette for todays date: " + paletteName)
            fetchPaletteToLightsFromSheet(paletteName)
            return
    #If no palettes are found matching todays date
    #MAKE SURE THAT PALETTES IN SHEET ARE IN FORMAT YYYY-MM-DD
    TODAYS_PALETTE = "None"
    print("No palette for today's date")

#Upper floor dictionary int to string
upper_floor_string_map = {
    5: "5",
    8: "8",
    9: "9",
    12: "12",
    13: "13",
    17: "17",
    18: "18",
    19: "19",
    20: "20",
    24: "24",
    25: "25",
    26: "26",
}

#Lower floor dictionary int to string
lower_floor_string_map = {
    22: "22",
    15: "15",
    10: "10",
    21: "21",
    7: "7",
    23: "23",
    16: "16",
    14: "14",
    11: "11",
}
#Input: Name of a palette matching one in the google sheet
#Pushes that palette to Philips Hue lights
#If Tic Tac Toe is running, doesn't push to lower floor
def fetchPaletteToLightsFromSheet(palette_name):
    print("Searching for palette: " + palette_name)

    palettes_from_sheet = sheet.get_all_records()

    for palette in palettes_from_sheet:
        if palette['PaletteName'] == palette_name:
            print("Found palette: " + palette_name)
            palette_area = palette['Area']
            print("Palette Area/zone: " + palette_area)
            global TICTACTOE_RUNNING
            if TICTACTOE_RUNNING:
                print("Not sending palette to lower floor b/c TTT is running")
            print("check")
            if((palette_area == "lower" or palette_area == "all") and not TICTACTOE_RUNNING):
                print("Sending to lower")
                for key in lower_floor_string_map:
                    print(key)
                    #print(lower_floor_string_map[key])
                    changeLight(key, 2, "on", "true", "hue", str(palette['Light ' + lower_floor_string_map[key] +' H']), "sat",
                                str(palette['Light ' + lower_floor_string_map[key] +' S']),
                                "bri", str(palette['Light ' + lower_floor_string_map[key] +' B']))
                print("Pushed palette - " + palette_name + " - to Philips Hue lights Lower Floor")
            if (palette_area == "upper" or palette_area == "all"):
                for key in upper_floor_string_map:
                    # print(key)
                    # print(upper_floor_string_map[key])
                    changeLight(key, 2, "on", "true", "hue",
                                str(palette['Light ' + upper_floor_string_map[key] + ' H']), "sat",
                                str(palette['Light ' + upper_floor_string_map[key] + ' S']),
                                "bri", str(palette['Light ' + upper_floor_string_map[key] + ' B']))
                print("Pushed palette - " + palette_name + " - to Philips Hue lights Upper Floor")



# Use a "main" function to run all code. Use a "while True" loop within main to repeat code
if __name__ == "__main__":
    # Run Once
    setup()
    # Initialize loop
    while True:
        #print("Starting loop!")
        loop()


