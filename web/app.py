from flask import Flask, render_template
from flask_bcrypt import Bcrypt
import config
import os

app = Flask(__name__, 
            template_folder=os.path.join(os.path.dirname(__file__), 'templates'),
            static_folder=os.path.join(os.path.dirname(__file__), 'static'))

app.config.from_object(config)

bcrypt = Bcrypt(app)

from blueprints import init_db
init_db(app)


from blueprints.main import main_bp
from blueprints.auth import auth_bp
from blueprints.capture import capture_bp
from blueprints.settings import settings_bp
from blueprints.user_management import user_mgmt_bp
app.register_blueprint(main_bp)
app.register_blueprint(auth_bp)
app.register_blueprint(capture_bp)
app.register_blueprint(settings_bp)
app.register_blueprint(user_mgmt_bp)

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)