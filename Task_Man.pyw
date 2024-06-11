import time, psutil, nvidia_smi, pystray, PIL, serial, tkinter, threading
from tkinter import simpledialog, messagebox
from PIL import Image, ImageTk
from sys import exit
import PIL.Image

# Insert header thing here

# The following setXxxx functions are used to load data from the config file, verify it, and store it in a global variable.

# Serial
def setSerial(valueIn):
    global device
    device = serial.Serial()
    device.baudrate = 9600
    device.port = valueIn
    print("\nInitializing serial at: " + device.port + "...")
    try:
        device.open()
        print("Opened serial at: " + device.port)
        return False
    except serial.serialutil.SerialException:
        print("Unable to open serial port!")
        error("Port: \"" + valueIn + "\"is invalid.", False)
        return True

# Refresh rate
def setRate(valueIn):
    global rate
    print("\nInitializing refresh rate...")
    try:
        rate = float(valueIn)
        print("\"" + valueIn + "\" is valid.")
        return False
    except ValueError:
        error("\"" + valueIn + "\"is invalid.", False)
        return True

# Index of the gpu
def setIndex(valueIn):
    global handle
    print("\nInitializing gpu...")
    try:
        nvidia_smi.nvmlInit()
        handle = nvidia_smi.nvmlDeviceGetHandleByIndex(int(valueIn))
        print("Index \"" + valueIn + "\" is valid.")
        return False
    except (nvidia_smi.NVMLError_InvalidArgument, ValueError):
        error("Index \"" + valueIn + "\"is invalid.", False)
        return True
    except (FileNotFoundError, nvidia_smi.NVMLError_LibraryNotFound):
        error("nvml.dll not found:\nCreate a folder NVSMI in C:\\Program Files\\NVIDIA Corporation\\\nThen copy nvml.dll from C:\\Windows\\System32 to C:\\Program Files\\NVIDIA Corporation\\NVSMI", False)
        return True

# Maximum read, write, download, and upload speed
def setRMax(valueIn):
    global rMax
    print("\nInitializing maximum read speed...")
    try:
        rMax = int(valueIn)
        print("\"" + valueIn + "\" is valid.")
        return False
    except ValueError:
        error("\"" + valueIn + "\"is invalid.", False)
        return True
def setWMax(valueIn):
    global wMax
    print("\nInitializing maximum write speed...")
    try:
        wMax = int(valueIn)
        print("\"" + valueIn + "\" is valid.")
        return False
    except ValueError:
        error("\"" + valueIn + "\"is invalid.", False)
        return True
def setDMax(valueIn):
    global dMax
    print("\nInitializing maximum download speed...")
    try:
        dMax = int(valueIn)
        print("\"" + valueIn + "\" is valid.")
        return False
    except ValueError:
        error("\"" + valueIn + "\"is invalid.", False)
        return True
def setUMax(valueIn):
    global uMax
    print("\nInitializing maximum read speed...")
    try:
        uMax = int(valueIn)
        print("\"" + valueIn + "\" is valid.")
        return False
    except ValueError:
        error("\"" + valueIn + "\"is invalid.", False)
        return True
        
# Disk name (name of disk or "ALL")
def setDiskName(valueIn):
    global diskName
    print("\nInitializing new disk...")
    if valueIn=="ALL":
        print("All disks selected. Systemwide totals will be used.")
        diskName = valueIn
        return False
    try:
        temp = psutil.disk_io_counters(perdisk=True)[valueIn]
        diskName = valueIn
        print("Disk \"" + valueIn + "\" is valid.")
        return False
    except KeyError:
        error("Disk name \"" + valueIn + "\" doesn\'t exist.", False)
        return True


# This function loads the data from the config file and passes it into the setXxxx functions.
# If the file is too short or isn't found, it tells the user.
def loadVars():
    print("Loading configuration file...")
    
    try:
        config = open("Task_Man_Config.txt", "r")
        configData = config.readlines()
        config.close()
        print("Config file found and read!")
        
        # Load the variables.
        if (setSerial(configData[0][:-1]) or setRate(configData[1][:-1]) or setIndex(configData[2][:-1]) or setRMax(configData[3][:-1]) or setWMax(configData[4][:-1]) or setDMax(configData[5][:-1]) or setUMax(configData[6][:-1]) or setDiskName(configData[7][:-1])):
            end()
        else:
            print("\nVariables initialized")
            
    except IndexError:
        print("\n")
        error("Config file is too short!")
        
    except FileNotFoundError:
        print("\n")
        error("Config file not found!")


# This function runs a while loop that gets the data and sends it to the device. 
def loop():
    global diskName, rate, handle, device

    # Monitoring variables
    cpu = 0
    gpu = 0
    ram = 0

    diskOldR = 0
    diskNewR = 0
    diskOldW = 0
    diskNewW = 0

    netOldD = 0
    netNewD = 0
    netOldU = 0
    netNewU = 0

    # serial data will look like this: "<C74|G23|M57|R20|W1|D102|U12|>"
    # Processed variables:
    C = 0 #cpu
    G = 0 #gpu
    M = 0 #ram
    R = 0 #disk read
    W = 0 #disk write
    D = 0 #net downstream
    U = 0 #net upstream
    output = ""

    # Monitor! (Forever)
    while not stop:
        if diskName=="ALL":
            diskOldR = psutil.disk_io_counters().read_bytes
            diskOldW = psutil.disk_io_counters().write_bytes
        else:
            diskOldR = psutil.disk_io_counters(perdisk=True)[diskName].read_bytes
            diskOldW = psutil.disk_io_counters(perdisk=True)[diskName].write_bytes
    
        netOldD = psutil.net_io_counters().bytes_recv
        netOldU = psutil.net_io_counters().bytes_sent
    
        # This also acts as a delay to allow for differences in the disk and net counters to be used to calculate data rates.
        cpu = psutil.cpu_percent(rate)
    
        if diskName=="ALL":
            diskNewR = psutil.disk_io_counters().read_bytes
            diskNewW = psutil.disk_io_counters().write_bytes
        else:
            diskNewR = psutil.disk_io_counters(perdisk=True)[diskName].read_bytes
            diskNewW = psutil.disk_io_counters(perdisk=True)[diskName].write_bytes
    
        netNewD = psutil.net_io_counters().bytes_recv
        netNewU = psutil.net_io_counters().bytes_sent
    
        gpu = nvidia_smi.nvmlDeviceGetUtilizationRates(handle).gpu

        ram = psutil.virtual_memory().percent

        # Process the data!
        C = int(cpu)
        G = int(gpu)
        M = int(ram)
        
        R = int((diskNewR-diskOldR)/1048576/rate)
        W = int((diskNewW-diskOldW)/1048576/rate)
        D = int((netNewD-netOldD)/1048576*8/rate)
        U = int((netNewU-netOldU)/1048576*8/rate)
    
        # This is the string that will be sent to the device.
        output = "<C"+str(C)+"|G"+str(G)+"|M"+str(M)+"|R"+str(R)+"|W"+str(W)+"|D"+str(D)+"|U"+str(U)+"|r"+str(rMax)+"|w"+str(wMax)+"|d"+str(dMax)+"|u"+str(uMax)+"|>"
        print(output)

        # Send the data over serial:
        try:
            device.write(output.encode("UTF-8"))
        except serial.serialutil.SerialException:
            error("Error while writing to serial port, accidental unplug? The program will now terminate.")


# This function alerts the user of an error and sometimes ends the program.
def error(message, endProgram = True):
    print("\n" + message)
    messagebox.showerror("Task Man", message)
    if endProgram:
        end()

# This function stops the program.
# It is called when the device is unplugged, when the user chooses the stop option with the tray icon, and when some errors occur.
def end():
    global stop, rate, icon, device
    print("\nStopping loop...")
    stop = True
    time.sleep(rate+1)
    print("Loop stopped.\nRemoving tray icon...")
    try:
        icon.stop()
    except AttributeError:
        pass
    print("Icon Removed.\nClosing serial...")
    try:
        device.close()
    except NameError:
        pass
    print("Serial closed.\nGoodbye!")
                

# When this variable is set to false the while loops stop looping
stop = False;


# This is the refresh rate in seconds,
# it is updated during the setRate() function.
rate = 1


# Icon, Tkinter, and Tray things:
try:
    with Image.open("TM.ico") as ico:

        # Tkinter things
        root = tkinter.Tk()
        root.withdraw()
        photo = ImageTk.PhotoImage(ico)
        root.wm_iconphoto(True, photo)

        # The tray icon
        icon = pystray.Icon("Task Man", ico, menu = pystray.Menu(pystray.MenuItem("Stop Task Man", end)))
except FileNotFoundError:
    error("Icon file not found! Refer to the github readme.", False)
    exit()

          
# Load the variables from the config file
loadVars()


# Start the icon
icon.run_detached()


# Run the loop
loop()

#TODO
#write readme
#publish
