import os
import sys

script_directory = os.path.dirname(os.path.abspath(sys.argv[0]))

script_path = os.path.join(script_directory, "web_resources", "html_to_c_string.py")
html_path = os.path.join(script_directory, "web_resources", "wifi_portal", "wifi_portal.html")
css_path = os.path.join(script_directory, "web_resources", "wifi_portal", "wifi_portal.css")
header_path = os.path.join(script_directory, "web_page.h")

os.system(sys.executable + " " + script_path + " " + html_path + " " + css_path + " > " + header_path);