import os
import sys
import json

num_args = len(sys.argv)
args = sys.argv

if num_args != 3:
    print("Invalid arguments!")
    exit(-1)

html_path = args[1]
css_path = args[2]


html_src = ""
css_src = ""

with open(html_path, 'r') as html_file:
    html_src = html_file.read()#.replace('\n', '').replace(' ', '')

with open(css_path, 'r') as css_file:
    css_src = css_file.read()#.replace('\n', '').replace(' ', '')

clean_html_src = json.dumps(html_src).strip('\"')
clean_css_src = json.dumps(css_src).strip('\"')

"""
HEAD_STR = '<head>'
head_idx = html_src.index(HEAD_STR) + len(HEAD_STR)
web_src = html_src[:head_idx] + "<style>" + css_src + "</style>"  + html_src[head_idx:]
clean_web_src = json.dumps(web_src).strip('\"')
"""

html_file_name = os.path.basename(html_path)
html_clean_file_name = os.path.splitext(html_file_name)[0].strip()
c_header_unique_id = html_clean_file_name.capitalize() + '_WEB_H'
c_header = '#ifndef ' + c_header_unique_id + '\n' + '#define ' + c_header_unique_id + '\n'
c_footer = '#endif\n'
c_string_html_def = 'const char* ' + html_clean_file_name.lower() + '_html_src = '
c_string_css_def = 'const char* ' + html_clean_file_name.lower() + '_css_src = '
c_src = c_header + '\n' + c_string_html_def + '\"' + clean_html_src + '\";\n\n' + c_string_css_def + '\"' + clean_css_src + '\";\n\n' + c_footer

print(c_src)