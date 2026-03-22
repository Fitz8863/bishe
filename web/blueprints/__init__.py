from flask_sqlalchemy import SQLAlchemy
from flask_login import LoginManager
from flask import redirect, url_for, request, jsonify

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
        # 允许的路由：登录、注册、静态文件、上传接口
        allowed_routes = ['auth.login', 'auth.register', 'static', 'capture.upload_capture', 'settings.intercom_webhook_stop']
        if request.endpoint in allowed_routes:
            return
        
        from flask_login import current_user
        if not current_user.is_authenticated:
            # API 路由返回 JSON 错误
            if request.path.startswith('/api/') or request.path.startswith('/settings/api/'):
                return jsonify({'error': 'Unauthorized'}), 401
            # 其他路由重定向到登录页
            return redirect(url_for('auth.login'))
    
    with app.app_context():
        db.create_all()
