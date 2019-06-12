#Interact w Hue
import requests

CONTROL_SIEG_LIGHTS = False

# IP Address of Philips Hue "Sieg Master" Bridge
sieg_master_ip = "172.28.219.179"
#sieg_master_ip = "10.0.1.16" #sam house

# API Token we have generated for Sieg Master Bridge
sieg_master_token = "rARKEpLebwXuW01cNVvQbnDEkd2bd56Nj-hpTETB"

# changeLight: Modify up to 4 parameters of a single light
# Parameter 1: lightNum - see mapping of Sieg lights: https://files.slack.com/files-tmb/TH0QLFCH3-FJGHX7K4P-6ecab43eeb/image_from_ios_720.jpg
# Paramter 2: Transitiontime (1/10ths of a second)
# Paramters 3 - 10: Names and values for Philiphs Hue Lighting paramters
# Example usage: changeLight(21,2,"on","true","hue", "30000")
# Turns light 21 to a greenish color

def changeLight(lightNum, transitiontime, parameter1, newValue1, parameter2=None, newValue2=None, parameter3=None,
                newValue3=None, parameter4=None, newValue4=None):

    req_string = "http://" + str(sieg_master_ip) + "/api/" + str(sieg_master_token) + "/lights/" + str(
        lightNum) + "/state"

    put_string = "{\"" + str(parameter1) + "\":" + str(newValue1) + ", \"transitiontime\":" + str(transitiontime)

    if parameter2 != None:
        put_string += ", \"" + parameter2 + "\": " + newValue2
    if parameter3 != None:
        put_string += ", \"" + parameter3 + "\" : " + newValue3
    if parameter4 != None:
        put_string += ", \"" + parameter4 + "\" : " + newValue4

    put_string += "}"

    print("Attempting light change for light: " + str(lightNum))
    try:
        if CONTROL_SIEG_LIGHTS:
            requests.put(req_string, put_string, verify=False, timeout=1)
    except requests.ConnectionError as e:
        print("Connection error: " + str(e))
        return
    except requests.HTTPError as e:
        print("HTTP error: " + str(e))
        return
    except requests.Timeout as e:
        print("Timeout error: " + str(e))
        return
    print("Success -  light change for light: " + str(lightNum))
    if not CONTROL_SIEG_LIGHTS:
        print("LIGHTS COMMANDS NOT SEND (CONTROL_SIEG_LIGHTS = False in Philips_Control_Functions.py.")
        print("Change to True to control lights. Must be connected to \"University of Washington\" wifi network.")


# changeGroup: Modify up to 4 parameters of a group of lights
# Parameter 1: groupNum - group 0 is all lights, group 1 is bottom floor, group 2 is top floor
# Paramter 2: Transitiontime (1/10ths of a second)
# Paramters 3 - 10: Names and values for Philiphs Hue Lighting paramters
# Example usage: changeGroup(1,2,"on","true","hue", "10000", "bri", "254")
# Turns bottom floor to a bright white
def changeGroup(groupNum, transitiontime, parameter1, newValue1, parameter2=None, newValue2=None, parameter3=None,
                newValue3=None, parameter4=None, newValue4=None):

    req_string = "http://" + str(sieg_master_ip) + "/api/" + str(sieg_master_token) + "/groups/" + str(
        groupNum) + "/action"

    put_string = "{\"" + str(parameter1) + "\":" + str(newValue1) + ", \"transitiontime\":" + str(transitiontime)

    if (parameter2 != None):
        put_string += ", \"" + parameter2 + "\": " + newValue2
    if (parameter3 != None):
        put_string += ", \"" + parameter3 + "\" : " + newValue3
    if (parameter4 != None):
        put_string += ", \"" + parameter4 + "\" : " + newValue4

    put_string += "}"

    print("Attempting group change for group number: " + str(groupNum))

    try:
        if CONTROL_SIEG_LIGHTS:
            requests.put(req_string, put_string, verify=False, timeout=1)
    except requests.ConnectionError as e:
        print("Connection error: " + str(e))
        return
    except requests.HTTPError as e:
        print("HTTP error: " + str(e))
        return
    except requests.Timeout as e:
        print("Timeout error: " + str(e))
        return
    print("Success -  group change for group number: " + str(groupNum))
    if not CONTROL_SIEG_LIGHTS:
        print("LIGHTS COMMANDS NOT SEND (CONTROL_SIEG_LIGHTS = False in Philips_Control_Functions.py.")
        print("Change to True to control lights. Must be connected to \"University of Washington\" wifi network.")