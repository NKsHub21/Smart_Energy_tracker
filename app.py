from flask import Flask, request, jsonify, send_file
from flask_cors import CORS # Needed browser security

import subprocess
import json
import os

app = Flask(__name__, static_url_path='') 
CORS(app) # Enables all routes

# Adjust C++ name
CPP_EXECUTABLE_NAME = 'energy_tracker' 
# Resolve executable path
CPP_EXECUTABLE_PATH = os.path.join(os.path.dirname(__file__), CPP_EXECUTABLE_NAME) 

# Windows check executable
if os.name == 'nt' and not os.path.exists(CPP_EXECUTABLE_PATH):
    CPP_EXECUTABLE_NAME = 'energy_tracker.exe'
    CPP_EXECUTABLE_PATH = os.path.join(os.path.dirname(__file__), CPP_EXECUTABLE_NAME)


@app.route('/')
def serve_index():
    # Serves index HTML
    return send_file('index.html')

@app.route('/api/calculate-energy', methods=['POST'])
def calculate_energy():
    # Get JSON input
    input_data_json = json.dumps(request.json)
    
    # Initialize report output
    report_json = "" 

    try:
        # Execute C++ program
        result = subprocess.run(
            [CPP_EXECUTABLE_PATH],
            input=input_data_json,
            capture_output=True,
            text=True,
            check=True
        )
        
        # Capture C++ report
        # MUST BE ALIGNED
        report_json = result.stdout
        
        # Parse return browser
        return jsonify(json.loads(report_json))

    except subprocess.CalledProcessError as e:
        # C++ crash error
        print(f"C++ Execution Failed (Return Code: {e.returncode})")
        print("C++ STDERR:", e.stderr)
        # Attempt C++ output
        try:
             # Process failed output
             error_output = json.loads(e.stdout) 
        except:
             error_output = {"error": "C++ calculation failed", "details": e.stderr}

        return jsonify(error_output), 500
    except json.JSONDecodeError:
        # C++ output invalid
        print("C++ output:", report_json)
        return jsonify({"error": "C++ returned invalid JSON", "output": report_json}), 500
    except FileNotFoundError:
        return jsonify({"error": f"C++ executable '{CPP_EXECUTABLE_NAME}' not found. Did you compile it correctly?"}), 500

if __name__ == '__main__':
    # Flask runs locally
    print(f"Starting Flask server on http://127.0.0.1:5000/")
    app.run(debug=True, port=5000)