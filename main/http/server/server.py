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
    if 'audio' not in request.files:
        return make_response({'error': 'No audio file part'}, 400)
    
    file = request.files['audio']
    
    if file.filename == '':
        return make_response({'error': 'No selected file'}, 400)
    
    if not (file and allowed_file(file.filename)):
        return make_response({'error': 'Invalid file type'}, 400)
    
    try:
        filename = secure_filename(file.filename)
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        unique_filename = f"{timestamp}_{filename}"
        
        os.makedirs(app.config['UPLOAD_FOLDER'], exist_ok=True)
        
        save_path = os.path.join(app.config['UPLOAD_FOLDER'], unique_filename)
        file.save(save_path)
        
        return make_response({
            'status': 'success',
            'filename': unique_filename,
            'message': 'File uploaded successfully'
        }, 200)
    
    except Exception as e:
        app.logger.error(f'Error uploading file: {str(e)}')
        return make_response({'error': 'File upload failed'}, 500)

if __name__ == '__main__':
    app.run(debug=True, port=5000)