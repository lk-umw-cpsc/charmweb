from mimetypes import init
from urllib import request
from flask import Flask, render_template, jsonify, request, session

import chemu

app = Flask(__name__)

app.config['SESSION_TYPE'] = 'memcached'
app.config['SECRET_KEY'] = 'super secret key'

def parse_instructions(instrs):
    for instruction in instrs:
        addr = instruction['instruction'][:10]
        instruction['instruction'] = instruction['instruction'][11:]
        instruction['address'] = addr
    return instrs

# method called when the app starts
@app.before_first_request
def initializer():
    result = chemu.init()
    instructions = session['instructions'] = parse_instructions(result['instructions'])
    registers = session['registers'] = ['0x00000000'] * 17
    output = session['output'] = []

    for s in result['output']:
        output.append(s)
    for reg in result['registers']:
        registers[reg['register']] = reg['value']
    session['initialized'] = True

@app.route('/', methods=['GET', 'POST'])
def home():
    # we receive POST if the user sent a command using the web interface, 
    #   GET if they simply loaded the page in their browser
    if request.method == 'POST':
        registers = session['registers']
        output = session['output']

        message = request.get_json()['message']

        # cap command to 32 characters (don't need massive strings)
        if len(message) > 32:
            s = s[:32]
        result = chemu.do(message)

        for reg in result['registers']:
            registers[reg['register']] = reg['value']

        instructions = session['instructions'] = parse_instructions(result['instructions'])

        for s in result['output']:
            output.append(s)
        
        return jsonify(register_updates=result['registers'], instruction_frame=result['instructions'], output=result['output'])
    else:
        instructions = session['instructions']
        registers = session['registers']
        output = session['output']
        print(registers)
        return render_template('index.html', registers=registers, instructions=instructions, output=output)
