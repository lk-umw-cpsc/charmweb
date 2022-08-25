from urllib import request
from flask import Flask, render_template, jsonify, request

import chemu

app = Flask(__name__)

# method called when the app starts
@app.before_first_request
def initializer():
    # chemu.init()
    pass

@app.route('/', methods=['GET', 'POST'])
def home():
    # we receive POST if the user sent a command using the web interface, 
    #   GET if they simply loaded the page in their browser
    if request.method == 'POST':
        message = request.get_json()['message']
        # cap command to 32 characters (don't need massive strings)
        if len(message) > 32:
            s = s[:32]
        result = chemu.do(message)

        for instruction in result['instructions']:
            addr = instruction['instruction'][:10]
            instruction['instruction'] = instruction['instruction'][11:]
            instruction['address'] = addr
        
        return jsonify(register_updates=result['registers'], instruction_frame=result['instructions'], output=result['output'])
    else:
        return render_template('index.html')
