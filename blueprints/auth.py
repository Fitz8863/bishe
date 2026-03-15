import functools
from flask import Blueprint, render_template, redirect, url_for, request, flash, current_app, abort
from flask_login import login_user, login_required, logout_user, current_user
from flask_bcrypt import Bcrypt
from .models import User
from . import db

auth_bp = Blueprint('auth', __name__, url_prefix='/')

def admin_required(f):
    @functools.wraps(f)
    def decorated_function(*args, **kwargs):
        if not current_user.is_authenticated or current_user.role not in ['admin', 'assistant']:
            abort(403)
        return f(*args, **kwargs)
    return decorated_function

def super_admin_required(f):
    @functools.wraps(f)
    def decorated_function(*args, **kwargs):
        if not current_user.is_authenticated or current_user.role != 'admin':
            abort(403)
        return f(*args, **kwargs)
    return decorated_function

@auth_bp.route('/register', methods=['GET', 'POST'])
def register():
    if current_user.is_authenticated:
        return redirect(url_for('main.index'))
    if request.method == 'POST':
        username = request.form.get('username')
        password = request.form.get('password')
        confirm_password = request.form.get('confirm_password')
        
        if password != confirm_password:
            flash('两次输入的密码不一致', 'danger')
            return render_template('register.html')
        
        bcrypt = Bcrypt(current_app._get_current_object())
        hashed_password = bcrypt.generate_password_hash(password).decode('utf-8')
        user = User(username=username, password=hashed_password, role='user')
        try:
            db.session.add(user)
            db.session.commit()
            flash('注册成功!请登录', 'success')
            return redirect(url_for('auth.login'))
        except:
            flash('用户名已存在', 'danger')
    return render_template('register.html')

@auth_bp.route('/login', methods=['GET', 'POST'])
def login():
    if current_user.is_authenticated:
        return redirect(url_for('main.index'))
    if request.method == 'POST':
        username = request.form.get('username')
        password = request.form.get('password')
        remember = request.form.get('remember') == 'on'
        
        bcrypt = Bcrypt(current_app._get_current_object())
        user = User.query.filter_by(username=username).first()
        if user and bcrypt.check_password_hash(user.password, password):
            login_user(user, remember=remember)
            return redirect(url_for('main.index'))
        else:
            flash('用户名或密码错误', 'danger')
    return render_template('login.html')

@auth_bp.route('/logout')
@login_required
def logout():
    logout_user()
    return redirect(url_for('main.index'))
