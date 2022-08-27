from math import isnan
from mimetypes import init
from urllib import request
from flask import Flask, render_template, jsonify, request, session

import chemu

app = Flask(__name__)

app.config['SESSION_TYPE'] = 'memcached'
app.config['SECRET_KEY'] = 'super secret key'

def init_regs():
    names = ['r'] * 13 + ['sp', 'lr', 'pc', 'cpsr']
    for i in range(13):
        names[i] += str(i)
    print(names)
    regs = []
    for name in names:
        reg = {}
        reg['value'] = '0x00000000'
        reg['int'] = 0
        reg['float'] = 0.0
        reg['name'] = name
        print(reg)
        regs.append(reg)
    return regs

def update_registers(regs):
    registers = session['registers']
    for reg in regs:
        fval = reg['float']
        if isnan(fval):
            fval = 'NaN'
        else:
            fval = str(round(fval, 2))
        ival = reg['int']
        hexval = reg['value']
        register = registers[reg['register']]
        register['int'] = ival
        register['float'] = reg['float'] = fval
        register['value'] = hexval
    return registers

def parse_instructions(instrs):
    for i, instruction_listing in enumerate(instrs):
        addr = instruction_listing['instruction'][:10]
        instr = instruction_listing['instruction'][12:]
        if i == 5:
            instr = instr[:-5]
        instruction_listing['instruction'] = instr
        instruction_listing['address'] = addr

    if instrs[5]['instruction'][0] == 'b':
        # we have a branch condition...
        for i in range(6, 11):
            instr = instrs[i]['instruction']
            split_point = instr.index(':', 6)
            if split_point < 0:
                continue
            left = instr[:split_point-10]
            right_add = instr[split_point-10:split_point]
            right_instr = instr[split_point+2:]
            instrs[i]['instruction'] = left
            right = { 'instruction': right_instr, 'address': right_add }
            instrs.append(right)
    fill = 16 - len(instrs)
    branch = fill == 0
    for i in range(fill):
        instrs.append({ 'address': '', 'instruction': '' })
    return instrs, branch

# method called when the app starts
@app.before_first_request
def initializer():
    result = chemu.init()
    instructions, branch = parse_instructions(result['instructions'])
    session['instructions'] = instructions
    registers = session['registers'] = init_regs()
    output = session['output'] = []

    for s in result['output']:
        output.append(s)
    output.append('% pl')
    update_registers(result['registers'])
    session['initialized'] = True

@app.route('/', methods=['GET', 'POST'])
def home():
    # we receive POST if the user sent a command using the web interface, 
    #   GET if they simply loaded the page in their browser
    if request.method == 'POST':
        output = session['output']

        message = request.get_json()['message']

        # cap command to 32 characters (don't need massive strings)
        if len(message) > 32:
            s = s[:32]
        result = chemu.do(message)

        registers = update_registers(result['registers'])
        
        branch_to_self = False
        if not result['registers']:
            # registers is empty if branch to self occurred
            branch_to_self = True
            branch = False
        else:
            for s in result['output']:
                output.append(s)
            instructions, branch = parse_instructions(result['instructions'])
            session['instructions'] = instructions
        print(result['registers'])
        return jsonify(register_updates=result['registers'], 
                instruction_frame=result['instructions'], 
                output=result['output'], 
                halt=branch_to_self, 
                branch=branch)
    else:
        instructions = session['instructions']
        fill = 16 - len(instructions)
        for i in range(fill):
            instructions.append({ 'address': '', 'instruction': '' })
        registers = session['registers']
        output = session['output']
        return render_template('index.html', registers=registers, instructions=instructions, output=output)
