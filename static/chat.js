document.addEventListener('DOMContentLoaded', function() {
    let currentRoom = '';
    let username = '';
    let isPolling = false;
    let retryDelay = 1000; // Start with 1 second delay

    // 检查本地存储的用户信息
    const savedUser = localStorage.getItem('chatUser');
    if (savedUser) {
        const userInfo = JSON.parse(savedUser);
        autoLogin(userInfo);
    }

    // Set up initial event listeners
    setupInitialEventListeners();

    function setupInitialEventListeners() {
        // Setup all event listeners with proper error handling
        setupElementListener('loginButton', 'click', login);
        setupElementListener('registerButton', 'click', register);
        setupElementListener('createRoomButton', 'click', createRoom);
        setupElementListener('sendButton', 'click', sendMessage);
        setupElementListener('messageInput', 'keypress', function(e) {
            if (e.key === 'Enter') {
                e.preventDefault();
                sendMessage();
            }
        });
        
        // Add room search event listener
        setupElementListener('roomSearchInput', 'input', searchRooms);
    }

    // Helper function to safely set up event listeners
    function setupElementListener(elementId, eventType, handler) {
        const element = document.getElementById(elementId);
        if (element) {
            element.addEventListener(eventType, handler);
        } else {
            console.warn(`Element with id '${elementId}' not found`);
        }
    }

    function autoLogin(userInfo) {
        fetch('/auto_login', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(userInfo)
        })
        .then(response => {
            if (!response.ok) {
                // 如果自动登录失败，清除本地存储
                localStorage.removeItem('chatUser');
                throw new Error('Auto login failed');
            }
            return response.json();
        })
        .then(data => {
            username = userInfo.username;
            
            // 隐藏登录表单
            document.getElementById('loginForm').style.display = 'none';
            
            // 显示聊天界面
            const chatInterface = document.getElementById('chatInterface');
            chatInterface.classList.add('active');
            document.body.style.padding = '0';
            document.body.style.background = 'white';
            
            // 更新欢迎信息
            document.getElementById('currentRoom').textContent = `Welcome, ${username}!`;
            
            // 加载房间列表
            loadRooms();
            
            // 开始轮询消息
            startPolling();
        })
        .catch(error => {
            console.error('Auto login error:', error);
            // 自动登录失败时不显示错误，只是显示登录表单
            document.getElementById('loginForm').style.display = 'block';
        });
    }

    function login() {
        username = document.getElementById('username').value;
        const password = document.getElementById('password').value;

        if (!username || !password) {
            alert('Please enter both username and password');
            return;
        }

        fetch('/login', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ username, password })
        })
        .then(response => {
            if (!response.ok) {
                throw new Error('Login failed');
            }
            return response.json();
        })
        .then(data => {
            // 保存用户信息到本地存储
            localStorage.setItem('chatUser', JSON.stringify({
                username: username,
                password: password
            }));
            
            // 隐藏登录表单
            document.getElementById('loginForm').style.display = 'none';
            
            // 显示聊天界面
            const chatInterface = document.getElementById('chatInterface');
            chatInterface.classList.add('active');
            document.body.style.padding = '0';
            document.body.style.background = 'white';
            
            // 更新欢迎信息
            document.getElementById('currentRoom').textContent = `Welcome, ${username}!`;
            
            // 加载房间列表
            loadRooms();
            
            // 开始轮询消息
            startPolling();
        })
        .catch(error => {
            console.error('Error:', error);
            alert('Login failed: ' + error.message);
        });
    }

    function register() {
        const username = document.getElementById('username').value;
        const password = document.getElementById('password').value;

        if (!username || !password) {
            alert('Please enter both username and password');
            return;
        }

        fetch('/register', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ username, password })
        })
        .then(response => {
            if (!response.ok) {
                throw new Error('Registration failed');
            }
            alert('Registration successful! Please login.');
        })
        .catch(error => {
            console.error('Error:', error);
            alert('Registration failed: ' + error.message);
        });
    }

    function loadRooms() {
        fetch('/rooms', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({})
        })
        .then(response => {
            if (!response.ok) {
                throw new Error('Failed to fetch rooms');
            }
            return response.json();
        })
        .then(rooms => {
            const roomList = document.getElementById('roomList');
            roomList.innerHTML = '';
            
            rooms.forEach(room => {
                const roomItem = document.createElement('div');
                roomItem.className = 'room-item';
                if (room.name === currentRoom) {
                    roomItem.classList.add('active');
                }
                
                const roomIcon = document.createElement('div');
                roomIcon.className = 'room-icon';
                roomIcon.textContent = room.name.charAt(0).toUpperCase();
                
                const roomInfo = document.createElement('div');
                roomInfo.className = 'room-info';
                
                const roomName = document.createElement('div');
                roomName.className = 'room-name';
                roomName.textContent = room.name;
                
                const roomMembers = document.createElement('div');
                roomMembers.className = 'room-members';
                roomMembers.textContent = `Created by: ${room.creator} | Members: ${room.members.length}`;
                
                roomInfo.appendChild(roomName);
                roomInfo.appendChild(roomMembers);
                
                roomItem.appendChild(roomIcon);
                roomItem.appendChild(roomInfo);
                
                roomItem.onclick = () => joinRoom(room.name);
                roomList.appendChild(roomItem);
            });
            
            // 在加载完房间后重新应用搜索过滤
            if (document.getElementById('roomSearchInput').value) {
                searchRooms();
            }
        })
        .catch(error => {
            console.error('Error fetching rooms:', error);
        });
    }

    function createRoom() {
        const roomName = document.getElementById('newRoomName').value;
        if (!roomName) {
            alert('Please enter a room name');
            return;
        }

        if (!username) {
            alert('Please login first');
            return;
        }

        fetch('/create_room', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ 
                name: roomName, 
                creator: username 
            })
        })
        .then(response => {
            if (!response.ok) {
                return response.json()
                    .then(data => {
                        throw new Error(data.error || 'Failed to create room');
                    })
                    .catch(() => {
                        return response.text().then(text => {
                            throw new Error(text || 'Failed to create room');
                        });
                    });
            }
            return response.json();
        })
        .then(data => {
            console.log('Room created:', data);
            document.getElementById('newRoomName').value = '';
            loadRooms();  // 重新加载房间列表
            // 自动加入新创建的房间
            joinRoom(roomName);
        })
        .catch(error => {
            console.error('Error:', error);
            alert(error.message || 'Failed to create room');
        });
    }

    function joinRoom(roomName) {
        if (!username) {
            alert('Please login first');
            return;
        }

        // 更新当前房间名
        currentRoom = roomName;
        document.getElementById('currentRoom').textContent = `Room: ${roomName}`;
        
        // 清空消息列表
        document.getElementById('messages').innerHTML = '';
        
        // 停止之前的轮询
        isPolling = false;
        
        fetch('/join_room', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                room: roomName,
                username: username
            })
        })
            .then(response => {
                if (!response.ok) {
                    return response.json().then(data => {
                        throw new Error(data.error || 'Failed to join room');
                    });
                }
                return response.json();
            })
            .then(data => {
                console.log('Joined room:', data);
                
                // 更新房间列表中的活动状态
                const roomItems = document.querySelectorAll('.room-item');
                roomItems.forEach(item => {
                    const nameDiv = item.querySelector('.room-name');
                    if (nameDiv && nameDiv.textContent === roomName) {
                        item.classList.add('active');
                    } else {
                        item.classList.remove('active');
                    }
                });
                
                // 显示历史消息
                if (data.messages && Array.isArray(data.messages)) {
                    data.messages.forEach(message => {
                        displayMessage(message);
                    });
                }
                
                // 开始轮询新消息
                startPolling();
            })
            .catch(error => {
                console.error('Error:', error);
                alert(error.message);
                // 重置当前房间
                currentRoom = '';
                document.getElementById('currentRoom').textContent = 'Welcome to Chat';
            });
    }

    function loadMessages(roomName) {
        fetch(`/messages?room=${encodeURIComponent(roomName)}`)
            .then(response => response.json())
            .then(messages => {
                const messagesDiv = document.getElementById('messages');
                messagesDiv.innerHTML = '';
                messages.forEach(message => {
                    displayMessage(message);
                });
                messagesDiv.scrollTop = messagesDiv.scrollHeight;
            })
            .catch(error => console.error('Error loading messages:', error));
    }

    function displayMessage(message) {
        const messagesDiv = document.getElementById('messages');
        const messageElement = document.createElement('div');
        messageElement.className = `message ${message.sender === username ? 'self' : 'other'}`;

        // 创建消息容器
        const messageContainer = document.createElement('div');
        messageContainer.className = 'message-container';

        // 如果是其他人的消息，显示发送者名字
        if (message.sender !== username) {
            const senderElement = document.createElement('div');
            senderElement.className = 'message-sender';
            senderElement.textContent = message.sender;
            messageContainer.appendChild(senderElement);
        }

        // 创建消息内容
        const contentElement = document.createElement('div');
        contentElement.className = 'message-content';
        
        // 处理消息文本，支持简单的表情符号
        const messageText = message.text.replace(/:\)|\(:|\:D|\:P/g, match => {
            const emojis = {
                ':)': '😊',
                '(:': '😊',
                ':D': '😃',
                ':P': '😛'
            };
            return emojis[match] || match;
        });
        
        contentElement.textContent = messageText;
        messageContainer.appendChild(contentElement);

        // 创建时间标签
        const timeElement = document.createElement('div');
        timeElement.className = 'message-time';
        const messageDate = new Date(message.timestamp);
        const now = new Date();
        
        // 格式化时间显示
        let timeText;
        if (messageDate.toDateString() === now.toDateString()) {
            // 今天的消息只显示时间
            timeText = messageDate.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
        } else {
            // 非今天的消息显示日期和时间
            timeText = messageDate.toLocaleDateString([], { 
                month: 'short', 
                day: 'numeric' 
            }) + ' ' + messageDate.toLocaleTimeString([], { 
                hour: '2-digit', 
                minute: '2-digit' 
            });
        }
        timeElement.textContent = timeText;
        messageContainer.appendChild(timeElement);

        // 将消息容器添加到消息元素中
        messageElement.appendChild(messageContainer);
        
        // 添加到消息列表
        messagesDiv.appendChild(messageElement);
        
        // 自动滚动到底部
        messagesDiv.scrollTop = messagesDiv.scrollHeight;
        
        // 清除浮动
        const clearDiv = document.createElement('div');
        clearDiv.style.clear = 'both';
        messagesDiv.appendChild(clearDiv);
    }

    function sendMessage() {
        if (!currentRoom) {
            alert('Please join a room first');
            return;
        }

        const messageInput = document.getElementById('messageInput');
        const messageText = messageInput.value.trim();
        if (!messageText) {
            return;
        }

        const message = {
            text: messageText,
            sender: username,
            timestamp: new Date().toISOString()
        };

        fetch(`/send_message?room=${encodeURIComponent(currentRoom)}`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(message)
        })
        .then(response => {
            if (!response.ok) {
                throw new Error('Failed to send message');
            }
            messageInput.value = '';
            // 立即显示发送的消息，不等待轮询
            displayMessage(message);
        })
        .catch(error => {
            console.error('Error:', error);
            alert('Failed to send message: ' + error.message);
        });
    }

    async function startPolling() {
        if (isPolling) return;
        isPolling = true;
        const pollingRoom = currentRoom; // Capture current room

        while (isPolling && currentRoom === pollingRoom) {
            if (!currentRoom) {
                await new Promise(resolve => setTimeout(resolve, 1000));
                continue;
            }

            try {
                const response = await fetch(`/poll?room=${encodeURIComponent(currentRoom)}&username=${encodeURIComponent(username)}`);
                if (!response.ok) {
                    throw new Error('Polling failed');
                }
                const messages = await response.json();
                
                if (messages && messages.length > 0) {
                    messages.forEach(message => displayMessage(message));
                    retryDelay = 1000; // Reset delay on successful message receipt
                } else {
                    // Gradually increase delay when no messages, up to 5 seconds
                    retryDelay = Math.min(retryDelay * 1.5, 5000);
                }
            } catch (error) {
                console.error('Polling error:', error);
                retryDelay = Math.min(retryDelay * 2, 5000); // Increase delay on error
            }

            await new Promise(resolve => setTimeout(resolve, retryDelay));
        }
        isPolling = false; // Reset polling state when loop exits
    }

    function setupMessageInput() {
        const messageInput = document.getElementById('messageInput');
        if (!messageInput) return;

        messageInput.addEventListener('input', function() {
            // 重置高度
            this.style.height = 'auto';
            // 设置新高度
            const newHeight = Math.min(this.scrollHeight, 120);
            this.style.height = newHeight + 'px';
        });

        // 处理按键事件
        messageInput.addEventListener('keydown', function(e) {
            if (e.key === 'Enter' && !e.shiftKey) {
                e.preventDefault();
                sendMessage();
            }
        });
    }

    function setUsername() {
        const input = document.getElementById('usernameInput');
        const newUsername = input.value.trim();
        
        if (!newUsername) {
            alert('Please enter a username');
            return;
        }
        
        // 先尝试注册
        fetch('/register', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                username: newUsername,
                password: 'default'  // 简单起见，使用默认密码
            })
        })
        .then(response => response.json())
        .then(() => {
            // 不管注册是否成功，都尝试登录
            return fetch('/login', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    username: newUsername,
                    password: 'default'
                })
            });
        })
        .then(response => response.json())
        .then(data => {
            if (data.success) {
                username = newUsername;
                document.getElementById('username').textContent = username;
                document.getElementById('loginForm').style.display = 'none';
                document.getElementById('chatInterface').style.display = 'block';
                loadRooms();
            } else {
                throw new Error(data.error || 'Failed to login');
            }
        })
        .catch(error => {
            console.error('Error:', error);
            alert(error.message);
        });
    }

    function searchRooms() {
        const searchInput = document.getElementById('roomSearchInput');
        const searchTerm = searchInput.value.toLowerCase();
        const roomItems = document.querySelectorAll('.room-item');

        roomItems.forEach(item => {
            const roomName = item.querySelector('.room-name').textContent.toLowerCase();
            const roomInfo = item.querySelector('.room-info').textContent.toLowerCase();
            
            if (roomName.includes(searchTerm) || roomInfo.includes(searchTerm)) {
                item.style.display = '';
            } else {
                item.style.display = 'none';
            }
        });
    }

    setupMessageInput();

    // Make functions available globally for inline button onclick handlers
    window.login = login;
    window.register = register;
    window.createRoom = createRoom;
    window.sendMessage = sendMessage;
    window.setUsername = setUsername;

});
