from flask import Flask, request, make_response
import os
from datetime import datetime
from werkzeug.utils import secure_filename

app = Flask(__name__)

UPLOAD_FOLDER = 'static/uploads'
ALLOWED_EXTENSIONS = {'wav'}
MAX_CONTENT_LENGTH = 16 * 1024 * 1024  # 16MB limit

app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER
app.config['MAX_CONTENT_LENGTH'] = MAX_CONTENT_LENGTH

def allowed_file(filename):
    return '.' in filename and \
           filename.rsplit('.', 1)[1].lower() in ALLOWED_EXTENSIONS

@app.route('/upload', methods=['POST'])
def uploadAudioFile():
    # Check if the request has data (raw upload)
    if not request.data:
        return make_response({'error': 'No file uploaded'}, 400)
    
    try:
        # Generate a unique filename (e.g., timestamp)
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        unique_filename = f"recording_{timestamp}.wav"
        
        os.makedirs(app.config['UPLOAD_FOLDER'], exist_ok=True)
        save_path = os.path.join(app.config['UPLOAD_FOLDER'], unique_filename)
        
        # Save raw request data to file
        with open(save_path, 'wb') as f:
            f.write(request.data)  # Raw bytes
        
        return make_response({
            'status': 'success',
            'filename': unique_filename,
            'message': 'File uploaded successfully'
        }, 200)
    
    except Exception as e:
        app.logger.error(f'Error uploading file: {str(e)}')
        return make_response({'error': 'File upload failed'}, 500)

if __name__ == '__main__':
    app.run(host="0.0.0.0", debug=True, port=5000)