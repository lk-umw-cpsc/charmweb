from urllib import request
from flask import Flask, render_template, jsonify, request
import random

import chemu

app = Flask(__name__)

@app.route('/', methods=['GET', 'POST'])
def home():
    if request.method == 'POST':
        message = request.get_json()['message']
        if message == 's':
            # reg = {}
            # reg['value'] = '0xfeedabee'
            # reg['register'] = random.randint(0, 15)
            # reg16 = {}
            # reg16['value'] = '0xFFFFFFFF'
            # reg16['register'] = 16
            # register_updates = [reg, reg16]
            # instruction = {}
            # instruction['address'] = '0x00abcdef'
            # instruction['instruction'] = 'test abc def'
            # instruction_frame = [instruction,instruction,instruction,instruction,instruction,instruction,instruction,instruction,instruction,instruction,instruction,instruction,instruction,instruction,instruction,instruction,instruction]
            result = chemu.do(message)
            return jsonify(register_updates=result['registers'], instruction_frame=result['instructions'])
    else:
        return render_template('index.html')