from flask import Flask, request, jsonify
import json

app = Flask(__name__)

# In-memory storage for received data
received_metrics = []
received_registrations = []

@app.route('/metrics', methods=['POST'])
def metrics():
    data = request.json
    print(f"Received metrics: {json.dumps(data, indent=2)}")
    received_metrics.append(data)
    return jsonify({"status": "ok"}), 201

@app.route('/register', methods=['POST'])
def register():
    data = request.json
    print(f"Received registration: {json.dumps(data, indent=2)}")
    received_registrations.append(data)
    return jsonify({"status": "ok"}), 201

@app.route('/verify', methods=['GET'])
def verify():
    return jsonify({
        "metrics_count": len(received_metrics),
        "registrations_count": len(received_registrations),
        "last_registration": received_registrations[-1] if received_registrations else None,
        "metrics": received_metrics
    }), 200

if __name__ == '__main__':
    # Listen on all interfaces
    app.run(host='0.0.0.0', port=5000)
