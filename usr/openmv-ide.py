#!/usr/bin/env python2
import openmv
import gtk
import gobject
import pango
import serial
import platform
import sys, os, os.path
from time import sleep
from os.path import expanduser
import gtksourceview2 as gtksourceview
from glob import glob
import urllib2, json
import numpy as np

#import pydfu on Linux
if platform.system() == "Linux":
    import pydfu

try:
    # 3.x name
    import configparser
except ImportError:
    # 2.x name
    configparser = __import__("ConfigParser")

if hasattr(sys,"frozen"):
    IDE_DIR=os.path.dirname(sys.executable)
    BUNDLE_DIR = sys._MEIPASS
else:
    IDE_DIR=os.path.dirname(os.path.realpath(__file__))
    BUNDLE_DIR = IDE_DIR

FIRMWARE_VERSION_MAJOR  = 1
FIRMWARE_VERSION_MINOR  = 2
FIRMWARE_VERSION_PATCH  = 0

DATA_DIR     = os.path.join(os.path.expanduser("~"), "openmv") #use home dir
SCRIPTS_DIR  = os.path.join(DATA_DIR, "scripts")
EXAMPLES_DIR = os.path.join(IDE_DIR, "examples")
GLADE_PATH   = os.path.join(BUNDLE_DIR, "openmv-ide.glade")
CONFIG_PATH  = os.path.join(DATA_DIR, "openmv.config")
UDEV_PATH    = "/etc/udev/rules.d/50-openmv.rules"

SCALE =1
RECENT_FILES_LIMIT=5
FLASH_OFFSETS= [0x08000000, 0x08004000, 0x08008000, 0x0800C000,
                0x08010000, 0x08020000, 0x08040000, 0x08060000,
                0x08080000, 0x080A0000, 0x080C0000, 0x080E0000]

DEFAULT_CONFIG='''\
[main]
board = OpenMV2
serial_port = /dev/openmvcam
recent =
last_fw_path =
baudrate = 921600
enable_jpeg = True
'''
CONFIG_KEYS = ['board', 'serial_port', 'recent', 'last_fw_path', 'baudrate', 'enable_jpeg']
RELEASE_TAG_NAME = 'v1.2'
RELEASE_URL = 'https://api.github.com/repos/openmv/openmv/releases/latest'

class OMVGtk:
    def __init__(self):
        #Set the Glade file
        self.builder = gtk.Builder()
        self.builder.add_from_file(GLADE_PATH)

        # get top window
        self.window = self.builder.get_object("top_window")

        # status bar stuff
        self.statusbar = self.builder.get_object("statusbar")
        self.statusbar_ctx = self.statusbar.get_context_id("default")

        # set buttons
        self.save_button = self.builder.get_object('save_file_toolbutton')
        self.connect_button = self.builder.get_object('connect_button')
        self.exec_button = self.builder.get_object('exec_button')
        self.stop_button = self.builder.get_object('stop_button')
        self.fwupdate_button = self.builder.get_object('bootloader_button')

        self.save_button.set_sensitive(False)
        self.exec_button.set_sensitive(False)
        self.stop_button.set_sensitive(False)
        self.connect_button.set_sensitive(True)

        # set control buttons
        self.controls = [
            self.builder.get_object('reset_button'),
            self.builder.get_object('bootloader_button'),
            self.builder.get_object('exec_button'),
            self.builder.get_object('zoomin_button'),
            self.builder.get_object('zoomout_button'),
            self.builder.get_object('bestfit_button'),
            self.builder.get_object('refresh_button')]

        self.connected = False
        map(lambda x:x.set_sensitive(False), self.controls)

        # Disable dfu button on Windows
        if platform.system() == "Windows":
            self.controls.pop(1)

        # gtksourceview widget
        sourceview = gtksourceview.View()
        lang_manager = gtksourceview.language_manager_get_default()
        style_manager = gtksourceview.style_scheme_manager_get_default()

        # append cwd to style search paths
        style_manager.set_search_path(style_manager.get_search_path() +
                [os.path.join(IDE_DIR, "share/gtksourceview-2.0/styles")])

        # append cwd to language search paths
        lang_manager.set_search_path(lang_manager.get_search_path() +
                [os.path.join(IDE_DIR, "share/gtksourceview-2.0/language-specs")])

        # configure gtksourceview widget
        sourceview.set_show_line_numbers(True)
        sourceview.set_tab_width(4)
        sourceview.set_indent_on_tab(True)
        sourceview.set_insert_spaces_instead_of_tabs(True)
        sourceview.set_auto_indent(True)
        sourceview.set_highlight_current_line(True)

        fonts = []
        for font in sourceview.get_pango_context().list_families():
            fonts.append(font.get_name().lower())

        if sys.platform.startswith("win"):
            if "consolas" in fonts:
                sourceview.modify_font(pango.FontDescription("consolas 10"))
            else:
                sourceview.modify_font(pango.FontDescription("courier new 10"))
        elif sys.platform.startswith("darwin"):
            if "monaco" in fonts:
                sourceview.modify_font(pango.FontDescription("monaco 10"))
            else:
                sourceview.modify_font(pango.FontDescription("courier new 10"))
        elif sys.platform.startswith("linux"):
            if "dejavu sans mono" in fonts:
                sourceview.modify_font(pango.FontDescription("dejavu sans mono 10"))
            else:
                sourceview.modify_font(pango.FontDescription("courier new 10"))

        # configure gtksourceview buffer
        self.buffer = gtksourceview.Buffer()
        self.buffer.set_highlight_syntax(True)
        self.buffer.set_language(lang_manager.get_language("python"))
        self.buffer.connect("changed", self.text_changed)

        sourceview.set_buffer(self.buffer)
        self.builder.get_object("src_scrolledwindow").add(sourceview)

        # Configure terminal window
        self.terminal_scroll = self.builder.get_object('vte_scrolledwindow')
        self.terminal = self.builder.get_object('vte_textview')
        self.terminal.modify_base(gtk.STATE_NORMAL, gtk.gdk.color_parse('black'))
        self.terminal.modify_text(gtk.STATE_NORMAL, gtk.gdk.color_parse('green'))

        # get drawingarea
        self.fb = None
        self.pixbuf = None
        self.drawingarea = self.builder.get_object("drawingarea")
        self.da_menu = self.builder.get_object("da_menu")

        # selection coords
        self.sel_ended=False
        self.selection_started=False
        self.x1 = self.y1 = self.x2 = self.y2 =0

        # set control scales attributes
        self.builder.get_object("contrast_adjust").attr=    openmv.ATTR_CONTRAST
        self.builder.get_object("brightness_adjust").attr=  openmv.ATTR_BRIGHTNESS
        self.builder.get_object("saturation_adjust").attr=  openmv.ATTR_SATURATION
        self.builder.get_object("gainceiling_adjust").attr= openmv.ATTR_GAINCEILING

        #connect signals
        signals = {
            "on_top_window_destroy"         : self.quit,
            "on_connect_clicked"            : self.connect_clicked,
            "on_reset_clicked"              : self.reset_clicked,
            "on_fwupdate_clicked"           : self.fwupdate_clicked,
            "on_fwpath_clicked"             : self.fwpath_clicked,
            "on_execute_clicked"            : self.execute_clicked,
            "on_stop_clicked"               : self.stop_clicked,
            "on_motion_notify"              : self.motion_notify,
            "on_button_press"               : self.button_pressed,
            "on_button_release"             : self.button_released,
            "on_open_file"                  : self.open_file,
            "on_new_file"                   : self.new_file,
            "on_save_file"                  : self.save_file,
            "on_save_file_as"               : self.save_file_as,
            "on_about_dialog"               : self.about_dialog,
            "on_pinout_dialog"              : self.pinout_dialog,
            "on_copy_color_activate"        : self.copy_color,
            "on_save_template_activate"     : self.save_template,
            "on_save_descriptor_activate"   : self.save_descriptor,
            "on_ctrl_scale_value_changed"   : self.on_ctrl_scale_value_changed,
            "on_zoomin_clicked"             : self.zoomin_clicked,
            "on_zoomout_clicked"            : self.zoomout_clicked,
            "on_bestfit_clicked"            : self.bestfit_clicked,
            "on_preferences_clicked"        : self.preferences_clicked,
            "on_updatefb_clicked"           : self.updatefb_clicked,
            "on_vte_size_allocate"          : self.scroll_terminal,
        }
        self.builder.connect_signals(signals)

        # create data directory
        if not os.path.isdir(DATA_DIR):
            os.makedirs(DATA_DIR)

        # create user scripts directory
        if not os.path.isdir(SCRIPTS_DIR):
            os.makedirs(SCRIPTS_DIR)

        # set config parser
        self.config = configparser.ConfigParser()

        config_valid = True

        # check if config file exists
        if os.path.isfile(CONFIG_PATH):
            try:
                # load config
                self.config.read(CONFIG_PATH)
            except Exception as e:
                print ("Failed to open config file %s"%(e))
                sys.exit(1)

            # Check config keys, if one is missing set invalid
            for key in CONFIG_KEYS:
                if not self.config.has_option('main', key):
                    config_valid = False
                    break
        else:
             config_valid = False

        # create fresh config if needed
        if config_valid == False:
            try:
                with open(CONFIG_PATH, "w") as f:
                    f.write(DEFAULT_CONFIG)
            except Exception as e:
                print ("Failed to create config file %s"%(e))
                sys.exit(1)

        # load or reload the config file
        try:
            self.config.read(CONFIG_PATH)
        except Exception as e:
            print ("Failed to open config file %s"%(e))
            sys.exit(1)

        # current file path
        self.file_path= None
        self.fw_file_path=""
        path = self.config.get("main", "last_fw_path")
        if os.path.isfile(path):
            self.fw_file_path = path

        # built-in examples menu
        submenu = gtk.Menu()
        menu = self.builder.get_object('example_menu')
        files = sorted(os.listdir(EXAMPLES_DIR))
        for f in files:
            if f.endswith(".py"):
                label = os.path.basename(f)
                mitem = gtk.MenuItem(label, use_underline=False)
                mitem.connect("activate", self.open_example, EXAMPLES_DIR)
                submenu.append(mitem)

        menu.set_submenu(submenu)

        # recent files menu
        self.files = []
        files =self.config.get("main", "recent")
        if files:
            self.files = files.split(',')
            self.update_recent_files()

        self.baudrate = int(self.config.get("main", "baudrate"))

        # set enable/disable JPEG
        self.enable_jpeg = self.config.get("main", "enable_jpeg") == 'True'

        # load Example0-Hello-World-Demo.py
        self._load_file(os.path.join(EXAMPLES_DIR, "Example0-Hello-World-Demo.py"))
        self.save_button.set_sensitive(False)

    def show_message_dialog(self, msg_type, msg):
        message = gtk.MessageDialog(parent=self.window, flags=gtk.DIALOG_DESTROY_WITH_PARENT,
                                    type=msg_type, buttons=gtk.BUTTONS_OK, message_format=msg)
        message.run()
        message.destroy()

    def refresh_gui(self, delay=0.0001, wait=0.0001):
        sleep(delay)
        gtk.main_iteration_do(block=False)
        sleep(wait)

    def connect(self):
        connected = False
        for i in range(10):
            try:
                # opens CDC port.
                # Set small timeout when connecting
                openmv.init(self.config.get("main", "serial_port"), baudrate=self.baudrate, timeout=0.050)
                connected = True
            except Exception as e:
                connected = False
                sleep(0.100)

        if not connected:
            # create fresh config if needed
            if platform.system() == "Linux" and not os.path.isfile(UDEV_PATH):
                error_msg = ("Failed to open serial port.\n"
                             "Please install OpenMV's udev rules first:\n\n"
                             "sudo cp openmv/udev/50-openmv.rules /etc/udev/rules.d/\n"
                             "sudo udevadm control --reload-rules\n\n")
            else:
                error_msg = ("Failed to open serial port.\n"
                             "Please check the preferences Dialog.\n")

            self.show_message_dialog(gtk.MESSAGE_ERROR,"%s%s"%(error_msg, e))
            return

        # Set higher timeout after connecting for lengthy transfers.
        openmv.set_timeout(1*2) # SD Cards can cause big hicups.

        # add terminal update callback
        gobject.gobject.timeout_add(30, omvgtk.update_terminal)

        # check firmware version
        self.fw_mismatch = False
        fw_ver = openmv.fw_version()
        ide_ver = (FIRMWARE_VERSION_MAJOR,
                   FIRMWARE_VERSION_MINOR,
                   FIRMWARE_VERSION_PATCH)

        print("fw_version:" + str(fw_ver))
        print("ide_version:" + str(ide_ver))
        if (fw_ver[0] != FIRMWARE_VERSION_MAJOR):
            # If ABI versions don't match, nothing todo here...
            self.show_message_dialog(gtk.MESSAGE_ERROR,
                            "Firmware ABI version mismatch!\n"
                            "Please update the IDE and/or FW manually\n")
            return
        elif (FIRMWARE_VERSION_MINOR > fw_ver[1]
                or FIRMWARE_VERSION_PATCH > fw_ver[2]):
            self.fw_mismatch = True
            self.show_message_dialog(gtk.MESSAGE_ERROR,
                    "Firmware version mismatch!\n"
                    "An older firmware version has been detected.\n"
                    "Please update the firmware!")

        if (self.fw_mismatch):
            self.connected = True
            self.connect_button.set_sensitive(False)
            self.fwupdate_button.set_sensitive(True)
            return

        # interrupt any running code
        openmv.stop_script()

        # set enable JPEG
        openmv.enable_jpeg(self.enable_jpeg)

        self.connected = True
        self._update_title()
        self.connect_button.set_sensitive(False)
        map(lambda x:x.set_sensitive(True), self.controls)

    def disconnect(self):
        try:
            # stop running code
            openmv.stop_script();
        except:
            pass

        self.connected = False
        self._update_title()
        self.stop_button.set_sensitive(False)
        self.connect_button.set_sensitive(True)
        map(lambda x:x.set_sensitive(False), self.controls)

    def connect_clicked(self, widget):
        self.connect()

    def fwpath_clicked(self, widget):
        fw_entry = self.builder.get_object("fw_entry")
        dialog = gtk.FileChooserDialog(title=None,action=gtk.FILE_CHOOSER_ACTION_OPEN,
                buttons=(gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL, gtk.STOCK_OPEN, gtk.RESPONSE_OK))
        dialog.set_default_response(gtk.RESPONSE_OK)
        dialog.set_current_folder(SCRIPTS_DIR)
        ff = gtk.FileFilter()
        ff.set_name("dfu")
        ff.add_pattern("*.bin") #TODO change to DFU
        dialog.add_filter(ff)

        if dialog.run() == gtk.RESPONSE_OK:
            fw_entry.set_text(dialog.get_filename())

        dialog.destroy()

    # Fake multitasking :P
    def fwupdate_task(self, state):
        if (state["init"]):
            pydfu.init()
            state["init"]=False
            state["erase"]=True
            state["bar"].set_text("Erasing...")
            return True
        elif (state["erase"]):
            page = state["page"]
            total = len(FLASH_OFFSETS)
            pydfu.page_erase(FLASH_OFFSETS[page])
            page +=1
            state["bar"].set_fraction(page/float(total))
            if (page == total):
                state["erase"] = False
                state["write"] = True
                state["bar"].set_text("Uploading...")
            state["page"] = page
            return True
        elif (state["write"]):
            buf = state["buf"]
            xfer_bytes = state["xfer_bytes"]
            xfer_total = state["xfer_total"]

            # Send chunk
            chunk = min (64, xfer_total-xfer_bytes)
            pydfu.write_page(buf[xfer_bytes:xfer_bytes+chunk], xfer_bytes)

            xfer_bytes += chunk
            state["xfer_bytes"] = xfer_bytes
            state["bar"].set_fraction(xfer_bytes/float(xfer_total))

            if (xfer_bytes == xfer_total):
                pydfu.exit_dfu()
                state["dialog"].hide()
                self.disconnect()
                return False

            return True

    def fwupdate_clicked(self, widget):
        if (self.connected):
            dialog = self.builder.get_object("fw_dialog")
            fw_entry = self.builder.get_object("fw_entry")
            fw_progress = self.builder.get_object("fw_progressbar")
            ok_button = self.builder.get_object("fw_ok_button")
            cancel_button = self.builder.get_object("fw_cancel_button")

            ok_button.set_sensitive(True)
            cancel_button.set_sensitive(True)
            dialog.set_transient_for(self.window);

            # default FW bin path
            fw_entry.set_text(self.fw_file_path)
            fw_progress.set_text("")
            fw_progress.set_fraction(0.0)

            if dialog.run() == gtk.RESPONSE_OK:
                ok_button.set_sensitive(False)
                cancel_button.set_sensitive(False)

                fw_path = fw_entry.get_text()
                try:
                    with open(fw_path, 'r') as f:
                        buf= f.read()
                except Exception as e:
                    dialog.hide()
                    self.show_message_dialog(gtk.MESSAGE_ERROR, "Failed to open file %s"%str(e))
                    return

                self.fw_file_path = fw_path
                self.config.set("main", "last_fw_path", fw_path)

                state={"init":True, "erase":False, "write":False,
                    "page":0, "buf":buf, "bar":fw_progress, "dialog":dialog,
                    "xfer_bytes":0, "xfer_total":len(buf)}

                # call dfu-util
                openmv.enter_dfu()
                sleep(1.0)
                gobject.gobject.idle_add(self.fwupdate_task, state)
            else:
                dialog.hide()

    def reset_clicked(self, widget):
        if (self.connected):
            openmv.reset()

    def execute_clicked(self, widget):
        buf = self.buffer.get_text(self.buffer.get_start_iter(), self.buffer.get_end_iter())
        # exec script
        openmv.exec_script(buf)
        self.exec_button.set_sensitive(False)
        self.stop_button.set_sensitive(True)

    def stop_clicked(self, widget):
        openmv.stop_script();
        self.exec_button.set_sensitive(True)
        self.stop_button.set_sensitive(False)

    def zoomin_clicked(self, widget):
        global SCALE
        SCALE+=1

    def zoomout_clicked(self, widget):
        global SCALE
        if SCALE>1:
            SCALE-=1

    def bestfit_clicked(self, widget):
        global SCALE
        SCALE=1

    def preferences_clicked(self, widget):
        board_combo = self.builder.get_object("board_combo")
        sport_combo = self.builder.get_object("sport_combo")
        baud_combo = self.builder.get_object("baud_combo")
        dialog = self.builder.get_object("preferences_dialog")
        jpeg_check = self.builder.get_object("jpeg_check")

        # Fill serial ports combo
        sport_combo.get_model().clear()
        serial_ports = self.list_serial_ports()
        for i in serial_ports:
            sport_combo.append_text(i)

        if len(serial_ports):
            sport_combo.set_active(0)

        jpeg_check.set_active(self.enable_jpeg)

        # Save config
        if dialog.run() == gtk.RESPONSE_OK:
            self.config.set("main", "board", board_combo.get_active_text())
            self.config.set("main", "serial_port", sport_combo.get_active_text())
            self.config.set("main", "baudrate", baud_combo.get_active_text())
            self.config.set("main", "enable_jpeg", jpeg_check.get_active())
            self.enable_jpeg = jpeg_check.get_active()
            self.save_config()

        # Enable/Disable JPEG
        if (self.connected):
            openmv.enable_jpeg(self.enable_jpeg)

        dialog.hide()

    def updatefb_clicked(self, widget):
        openmv.fb_update()

    def button_pressed(self, widget, event):
        self.x1 = int(event.x)
        self.y1 = int(event.y)
        self.x2 = int(event.x)
        self.y2 = int(event.y)
        self.selection_started = True

    def button_released(self, widget, event):
        self.x2 = int(event.x)
        self.y2 = int(event.y)
        self.selection_started = False
        if (self.connected):
            self.da_menu.popup(None, None, None, event.button, event.time, None)
            self.da_menu.show_all()

    def motion_notify(self, widget, event):
        x = int(event.x)
        y = int(event.y)
        self.x2 = int(event.x)
        self.y2 = int(event.y)
        if self.pixbuf and x < self.pixbuf.get_width() and y < self.pixbuf.get_height():
            pixel = self.pixbuf.get_pixels_array()[y][x]
            rgb = "(%d, %d, %d)" %(pixel[0], pixel[1], pixel[2])
            self.statusbar.pop(self.statusbar_ctx)
            self.statusbar.push(self.statusbar_ctx, rgb)

    def scroll_terminal(self, widget, event):
        adj = self.terminal_scroll.get_vadjustment()
        adj.set_value(adj.upper - adj.page_size)

    def update_terminal(self):
        if (self.connected and not self.fw_mismatch):
            try:
                buf_len = openmv.tx_buf_len()
                if (buf_len):
                    buf = openmv.tx_buf(buf_len)
                    buffer = self.terminal.get_buffer()
                    buffer.insert(buffer.get_end_iter(), buf)
            except:
                pass

        return True

    def update_drawing(self):
        fb = None
        if (self.connected and not self.fw_mismatch):
            try:
                # read drawingarea
                fb = openmv.fb_dump()
            except Exception as e:
                self.disconnect()
                self._update_title()
                return True
        if fb:
            self.fb = fb
        else:
            fb = self.fb

        if fb:
            # create pixbuf from np array
            self.pixbuf = gtk.gdk.pixbuf_new_from_array(fb[2], gtk.gdk.COLORSPACE_RGB, 8)
            self.pixbuf = self.pixbuf.scale_simple(fb[0]*SCALE, fb[1]*SCALE, gtk.gdk.INTERP_BILINEAR)

            self.drawingarea.realize();
            cm = self.drawingarea.window.get_colormap()
            gc = self.drawingarea.window.new_gc(foreground=cm.alloc_color('#FFFFFF',True,False))

            self.drawingarea.set_size_request(fb[0]*SCALE, fb[1]*SCALE)
            self.drawingarea.window.draw_pixbuf(gc, self.pixbuf, 0, 0, 0, 0)
            if self.selection_started or self.da_menu.flags() & gtk.MAPPED:
                self.drawingarea.window.draw_rectangle(gc, False, self.x1, self.y1, self.x2-self.x1, self.y2-self.y1)

        return True

    def update_exec_button(self):
        if (self.connected and not self.fw_mismatch):
            try:
                # read drawingarea
                running = (openmv.script_running()==1)
                self.stop_button.set_sensitive(running)
                self.exec_button.set_sensitive(not running)
            except Exception as e:
                self.disconnect()
                self._update_title()

        return True

    def on_ctrl_scale_value_changed(self, adjust):
        openmv.set_attr(adjust.attr, int(adjust.value))

    def save_config(self):
        # config.set("section", "key", value)
        self.config.set("main", "recent", ','.join(self.files))
        with open(CONFIG_PATH, "w") as file:
           self.config.write(file)

    def _update_title(self):
        if (self.file_path==None):
            title = "Untitled"
        else:
            title = os.path.basename(self.file_path)

        title += " [Connected]" if self.connected else " [Disconnected]"
        self.window.set_title(title)


    def update_recent_files(self):
        if (self.file_path and self.file_path not in self.files ):
            self.files.insert(0, self.file_path)

        if len(self.files)>RECENT_FILES_LIMIT:
            self.files.pop()

        submenu = gtk.Menu()
        menu = self.builder.get_object('recent_menu')
        for f in self.files:
            if f.endswith(".py"):
                mitem =gtk.MenuItem(f, use_underline=False)
                mitem.connect("activate", self.open_example, "")
                submenu.append(mitem)

        menu.set_submenu(submenu)
        menu.show_all()

    def _load_file(self, path):
        self.file_path = path
        if path == None: # New file
            self.save_button.set_sensitive(True)
            self.buffer.set_text("")
        else:
            self.save_button.set_sensitive(False)
            with open(path, "r") as file:
                self.buffer.set_text(file.read())
            self.update_recent_files()
        self._update_title()

    def _save_file(self, new_file):
        if new_file:
            dialog = gtk.FileChooserDialog(title=None,action=gtk.FILE_CHOOSER_ACTION_SAVE,
                    buttons=(gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL, gtk.STOCK_OPEN, gtk.RESPONSE_OK))
            dialog.set_default_response(gtk.RESPONSE_OK)
            dialog.set_current_folder(SCRIPTS_DIR)
            ff = gtk.FileFilter()
            ff.set_name("python")
            ff.add_pattern("*.py")
            dialog.add_filter(ff)

            if dialog.run() == gtk.RESPONSE_OK:
                self.file_path = dialog.get_filename()
                self.save_button.set_sensitive(False)
                self._update_title()
                self.update_recent_files()

                # append .py
                filename = dialog.get_filename()
                if not filename.endswith(".py"):
                    filename += ".py"

                # save file
                with open(filename, "w") as file:
                    file.write(self.buffer.get_text(self.buffer.get_start_iter(), self.buffer.get_end_iter()))

            dialog.destroy()
        else:
            self.save_button.set_sensitive(False)
            with open(self.file_path, "w") as file:
                file.write(self.buffer.get_text(self.buffer.get_start_iter(), self.buffer.get_end_iter()))

    def copy_color(self, widget):
        self.da_menu.hide()
        x = self.x1
        y = self.y1
        w = self.x2-self.x1
        h = self.y2-self.y1

        def rgb2lab(rgb):

            def lin(c):
                return 100 * ((c/12.92) if (c<=0.04045) else pow((c+0.055)/1.055, 2.4))

            def f(t):
                return pow(t, (1/3.0)) if (t>0.008856) else ((7.787037*t)+0.137931)

            r_lin = lin(rgb[0] / 255.0)
            g_lin = lin(rgb[1] / 255.0)
            b_lin = lin(rgb[2] / 255.0)

            x = (r_lin * 0.4124) + (g_lin * 0.3576) + (b_lin * 0.1805);
            y = (r_lin * 0.2126) + (g_lin * 0.7152) + (b_lin * 0.0722);
            z = (r_lin * 0.0193) + (g_lin * 0.1192) + (b_lin * 0.9505);

            x = f(x / 095.047)
            y = f(y / 100.000)
            z = f(z / 108.883)

            l = int(round(116 * y)) - 16;
            a = int(round(500 * (x-y)));
            b = int(round(200 * (y-z)));

            return (l, a, b)

        def rgb2gry(rgb):

            def lin(c):
                return 100 * ((c/12.92) if (c<=0.04045) else pow((c+0.055)/1.055, 2.4))

            def f(t):
                return (1.055*pow(t, (1/2.4)))-0.055 if (t>0.0031308) else 12.92*t

            r_lin = lin(rgb[0] / 255.0)
            g_lin = lin(rgb[1] / 255.0)
            b_lin = lin(rgb[2] / 255.0)

            y = f(((r_lin * 0.2126) + (g_lin * 0.7152) + (b_lin * 0.0722)) / 100.0);
            return max(min(y * 255, 255), 0)

        def stats(buf, f):
            new_buf = np.zeros((buf.shape[0], buf.shape[1]), int)
            hist = np.zeros(384, int)
            for i in range(buf.shape[0]):
                for j in range(buf.shape[1]):
                    color = f(buf[i][j])
                    new_buf[i][j] = color
                    hist[color + 128] += 1
            return (np.mean(new_buf),
                    np.median(new_buf),
                    np.argmax(hist) - 128,
                    np.std(new_buf),
                    np.amin(new_buf),
                    np.amax(new_buf),
                    np.percentile(new_buf, 25),
                    np.percentile(new_buf, 75))

        buf = self.pixbuf.subpixbuf(x, y, w, h).get_pixels_array()
        r_stats = stats(buf, lambda x: x[0])
        g_stats = stats(buf, lambda x: x[1])
        b1stats = stats(buf, lambda x: x[2])
        l_stats = stats(buf, lambda x: rgb2lab(x)[0])
        a_stats = stats(buf, lambda x: rgb2lab(x)[1])
        b2stats = stats(buf, lambda x: rgb2lab(x)[2])
        y_stats = stats(buf, lambda x: rgb2gry(x))
        out = [r_stats, g_stats, b1stats, l_stats, a_stats, b2stats, y_stats]

        rgb_thresholds = [max(r_stats[6] - r_stats[3]*3, 0),
                          min(r_stats[7] + r_stats[3]*3, 255),
                          max(g_stats[6] - g_stats[3]*3, 0),
                          min(g_stats[7] + g_stats[3]*3, 255),
                          max(b1stats[6] - b1stats[3]*3, 0),
                          min(b1stats[7] + b1stats[3]*3, 255)]
        lab_thresholds = [max(l_stats[6] - l_stats[3]*3, 0),
                          min(l_stats[7] + l_stats[3]*3, 100),
                          max(a_stats[6] - a_stats[3]*3, -128),
                          min(a_stats[7] + a_stats[3]*3, 127),
                          max(b2stats[6] - b2stats[3]*3, -128),
                          min(b2stats[7] + b2stats[3]*3, 127)]
        gry_thresholds = [max(y_stats[6] - y_stats[3]*3, 0),
                          min(y_stats[7] + y_stats[3]*3, 255)]
        out.extend([rgb_thresholds, lab_thresholds, gry_thresholds])

        self.buffer.begin_user_action()
        iter = self.buffer.get_iter_at_mark(self.buffer.get_mark("insert"))
        iter.forward_line()
        self.buffer.insert(iter,\
        "# = RGB Color Space Stats ========================\n"\
        "# R: Mean %4d, Median %4d, Mode %4d, Stdev %4d\n"\
        "#    Min  %4d, Max    %4d, LQ   %4d, UQ    %4d\n"\
        "# G: Mean %4d, Median %4d, Mode %4d, Stdev %4d\n"\
        "#    Min  %4d, Max    %4d, LQ   %4d, UQ    %4d\n"\
        "# B: Mean %4d, Median %4d, Mode %4d, Stdev %4d\n"\
        "#    Min  %4d, Max    %4d, LQ   %4d, UQ    %4d\n"\
        "# = LAB Color Space Stats ========================\n"\
        "# L: Mean %4d, Median %4d, Mode %4d, Stdev %4d\n"\
        "#    Min  %4d, Max    %4d, LQ   %4d, UQ    %4d\n"\
        "# A: Mean %4d, Median %4d, Mode %4d, Stdev %4d\n"\
        "#    Min  %4d, Max    %4d, LQ   %4d, UQ    %4d\n"\
        "# B: Mean %4d, Median %4d, Mode %4d, Stdev %4d\n"\
        "#    Min  %4d, Max    %4d, LQ   %4d, UQ    %4d\n"\
        "# = GRY Color Space Stats ========================\n"\
        "#    Mean %4d, Median %4d, Mode %4d, Stdev %4d\n"\
        "#    Min  %4d, Max    %4d, LQ   %4d, UQ    %4d\n"\
        "# = Suggested Thresholds (LQ-Stdev*3, UQ+Stdev*3) \n"\
        "# Use LAB for RGB565 and GRY for GRAYSCALE images.\n"\
        "# RGB = (%4d, %4d, %4d, %4d, %4d, %4d)\n"\
        "# LAB = (%4d, %4d, %4d, %4d, %4d, %4d)\n"\
        "# GRY = (%4d, %4d)\n"\
        "# ================================================\n"\
        % tuple([i for sub in out for i in sub]))
        self.buffer.end_user_action()

    def save_template(self, widget):
        self.da_menu.hide()
        x = self.x1
        y = self.y1
        w = self.x2-self.x1
        h = self.y2-self.y1

        entry = self.builder.get_object("template_entry")
        image = self.builder.get_object("template_image")
        image.set_from_pixbuf(self.pixbuf.subpixbuf(x, y, w, h))

        dialog = self.builder.get_object("save_template_dialog")
        dialog.set_transient_for(self.window);
        #dialog.set_default_response(gtk.RESPONSE_OK)

        if dialog.run() == gtk.RESPONSE_OK:
            openmv.save_template(x/SCALE, y/SCALE, w/SCALE, h/SCALE, entry.get_text()) #Use Scale
        dialog.hide()

    def save_descriptor(self, widget):
        self.da_menu.hide()
        x = self.x1
        y = self.y1
        w = self.x2-self.x1
        h = self.y2-self.y1

        entry = self.builder.get_object("desc_entry")
        image = self.builder.get_object("desc_image")
        image.set_from_pixbuf(self.pixbuf.subpixbuf(x, y, w, h))

        dialog = self.builder.get_object("save_descriptor_dialog")
        dialog.set_transient_for(self.window);
        #dialog.set_default_response(gtk.RESPONSE_OK)

        if dialog.run() == gtk.RESPONSE_OK:
            #if not entry.get_text():
            openmv.save_descriptor(x/SCALE, y/SCALE, w/SCALE, h/SCALE, entry.get_text()) #Use Scale
        dialog.hide()

    def new_file(self, widget):
        self._load_file(None)

    def save_file(self, widget):
        self._save_file(self.file_path==None)

    def save_file_as(self, widget):
        self._save_file(True)

    def open_file(self, widget):
        dialog = gtk.FileChooserDialog(title=None,action=gtk.FILE_CHOOSER_ACTION_OPEN,
                buttons=(gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL, gtk.STOCK_OPEN, gtk.RESPONSE_OK))
        dialog.set_default_response(gtk.RESPONSE_OK)
        dialog.set_current_folder(SCRIPTS_DIR)
        ff = gtk.FileFilter()
        ff.set_name("python")
        ff.add_pattern("*.py")
        dialog.add_filter(ff)

        if dialog.run() == gtk.RESPONSE_OK:
            self._load_file(dialog.get_filename())

        dialog.destroy()

    def open_example(self, widget, basedir):
        self.file_path = os.path.join(basedir, widget.get_label())
        self._load_file(self.file_path)

    def about_dialog(self, widget):
        dialog = self.builder.get_object("about_dialog")
        ide_version = "v%d.%d.%d"%(FIRMWARE_VERSION_MAJOR, FIRMWARE_VERSION_MINOR, FIRMWARE_VERSION_PATCH)
        dialog.set_version(ide_version)
        dialog.run()
        dialog.hide()

    def pinout_dialog(self, widget):
        dialog = self.builder.get_object("pinout_dialog")
        dialog.run()
        dialog.hide()

    def text_changed(self, widget):
        self.save_button.set_sensitive(True)

    def list_serial_ports(self):
        serial_ports = []
        system_name = platform.system()

        if system_name == "Linux":
            serial_ports.append("/dev/openmvcam")
        elif system_name == "Darwin":
            serial_ports.extend(glob('/dev/tty.*'))
        elif system_name == "Windows":
            for i in range(256):
                try:
                    port = "COM%d"%i
                    s = serial.Serial(port)
                    serial_ports.append(port)
                    s.close()
                except serial.SerialException:
                    pass

        return serial_ports

    def check_for_updates(self):
        try:
            url = urllib2.urlopen(RELEASE_URL)
            release = json.loads(url.read())
            url.close()
            if (release['tag_name'] != RELEASE_TAG_NAME):
                dialog = self.builder.get_object("update_dialog")
                dn_button = self.builder.get_object("download_button")

                # Set release notes
                self.builder.get_object("rn_label").\
                set_text('Release notes (%s):\n\n%s'%(release['tag_name'], release['body']))
                # Set URL
                dn_button.set_uri(release['html_url'])
                dialog.run()
                dialog.hide()
        except:
            pass #pass quietly

    def quit(self, widget):
        try:
            # disconnect
            self.disconnect()
        except:
            pass

        self.save_config()
        # exit
        gtk.main_quit()

if __name__ == "__main__":
    omvgtk = OMVGtk()
    omvgtk.window.show_all()
    omvgtk.check_for_updates()
    gobject.gobject.timeout_add(30, omvgtk.update_drawing)
    gobject.gobject.timeout_add(500, omvgtk.update_exec_button)
    gtk.main()
