from flask_sqlalchemy import SQLAlchemy
from flask_login import LoginManager
from flask import redirect, url_for, request

db = SQLAlchemy()
login_manager = LoginManager()

def init_db(app):
    db.init_app(app)
    login_manager.init_app(app)
    login_manager.login_view = 'auth.login'
    login_manager.login_message = "请先登录以访问系统"
    
    from .models import User
    
    @login_manager.user_loader
    def load_user(user_id):
        return User.query.get(int(user_id))
    
    @app.before_request
    def check_login():
        allowed_routes = ['auth.login', 'auth.register', 'static']
        if request.endpoint in allowed_routes:
            return
        
        if request.endpoint == 'capture.upload':
            return

        from flask_login import current_user
        if not current_user.is_authenticated:
            if request.path.startswith('/api/') or request.path.startswith('/settings/api/') or request.path.startswith('/capture/'):
                 from flask import jsonify
                 if request.endpoint != 'capture.upload':
                    return jsonify({'error': 'Unauthorized'}), 401
            
            return redirect(url_for('auth.login'))
    
    with app.app_context():
        db.create_all()
