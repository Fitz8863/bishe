# 存在就是为了解决循环引用的问题
# 第三方插件
from flask_sqlalchemy import SQLAlchemy
from flask_mail import Mail
from flask_socketio import SocketIO
from flask_mqtt import Mqtt

# 创建 socketio 实例
socketio = SocketIO(cors_allowed_origins="*")  # 允许跨域
db = SQLAlchemy()
mail = Mail()

mqtt = Mqtt()

