document.addEventListener('DOMContentLoaded', async () => {
    let currentRoom = null;
    let username = localStorage.getItem('username');
    let currentMessages = [];
    let retryDelay = 1000; // Start with 1 second delay
    let isPolling = false;
    
    // 获取 DOM 元素
    const loginForm = document.getElementById('loginForm');
    const chatInterface = document.getElementById('chatInterface');
    const usernameInput = document.getElementById('username');
    const passwordInput = document.getElementById('password');
    const loginButton = document.getElementById('loginButton');
    const registerButton = document.getElementById('registerButton');
    
    // 尝试自动登录
    async function tryAutoLogin() {
        if (!username) return false;
        
        try {
            const response = await fetch('/auto_login', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({ username })
            });
            
            if (response.ok) {
                const data = await response.json();
                if (data.status === 'success') {
                    loginForm.style.display = 'none';
                    chatInterface.style.display = 'flex';
                    return true;
                }
            }
            // 如果自动登录失败，清除存储的用户名
            localStorage.removeItem('username');
            username = null;
            return false;
        } catch (error) {
            console.error('Auto login error:', error);
            localStorage.removeItem('username');
            username = null;
            return false;
        }
    }
    
    // 如果已经登录，尝试自动登录
    if (username) {
        tryAutoLogin().then(success => {
            if (success) {
                initialize().catch(error => {
                    console.error('Fatal error:', error);
                    alert('Failed to start chat. Please refresh the page.');
                });
            } else {
                loginForm.style.display = 'block';
                chatInterface.style.display = 'none';
            }
        });
    } else {
        loginForm.style.display = 'block';
        chatInterface.style.display = 'none';
    }
    
    // 登录处理
    async function handleLogin() {
        const username = usernameInput.value.trim();
        const password = passwordInput.value.trim();
        
        if (!username || !password) {
            alert('Please enter both username and password');
            return;
        }
        
        try {
            const response = await fetch('/login', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({ username, password })
            });
            
            const data = await response.json();
            
            if (response.ok) {
                localStorage.setItem('username', data.username);
                username = data.username;  // 更新当前用户名
                loginForm.style.display = 'none';
                chatInterface.style.display = 'flex';
                await initialize();
            } else {
                alert(data.error || 'Login failed');
            }
        } catch (error) {
            console.error('Login error:', error);
            alert('Login failed. Please try again.');
        }
    }
    
    // 注册处理
    async function handleRegister() {
        const username = usernameInput.value.trim();
        const password = passwordInput.value.trim();
        
        if (!username || !password) {
            alert('Please enter both username and password');
            return;
        }
        
        try {
            const response = await fetch('/register', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({ username, password })
            });
            
            const data = await response.json();
            
            if (response.ok) {
                alert('Registration successful! Please login.');
                usernameInput.value = '';
                passwordInput.value = '';
            } else {
                alert(data.error || 'Registration failed');
            }
        } catch (error) {
            console.error('Registration error:', error);
            alert('Failed to register. Please try again.');
        }
    }
    
    // 添加事件监听器
    loginButton.addEventListener('click', handleLogin);
    registerButton.addEventListener('click', handleRegister);
    
    // 添加回车键监听
    usernameInput.addEventListener('keypress', (e) => {
        if (e.key === 'Enter') {
            passwordInput.focus();
        }
    });
    
    passwordInput.addEventListener('keypress', (e) => {
        if (e.key === 'Enter') {
            handleLogin();
        }
    });
    
    // Poll users
    async function pollUsers() {
        try {
            console.log('Polling users...');
            const response = await fetch('/users');
            if (response.ok) {
                const users = await response.json();
                console.log('Received users:', users);
                displayUsers(users);
            } else {
                console.error('Failed to fetch users:', response.status);
            }
        } catch (error) {
            console.error('Error polling users:', error);
        }
    }
    
    // Display users
    function displayUsers(users) {
        console.log('Displaying users:', users);
        const userList = document.getElementById('userList');
        userList.innerHTML = '';
        
        users.forEach(user => {
            const userDiv = document.createElement('div');
            userDiv.className = 'user-item';
            
            const nameDiv = document.createElement('div');
            nameDiv.className = 'name';
            nameDiv.textContent = user.username;
            
            // 高亮当前用户
            if (user.username === localStorage.getItem('username')) {
                nameDiv.style.fontWeight = 'bold';
                nameDiv.textContent += ' (You)';
            }
            
            userDiv.appendChild(nameDiv);
            userList.appendChild(userDiv);
        });
    }
    
    // Format timestamp
    function formatTimestamp(timestamp) {
        const date = new Date(parseInt(timestamp));
        return date.toLocaleTimeString([], { 
            hour: '2-digit', 
            minute: '2-digit',
            hour12: false
        });
    }
    
    // Display messages
    function displayMessages(messages) {
        const messagesDiv = document.getElementById('messages');
        messagesDiv.innerHTML = '';
        
        messages.forEach(message => {
            const messageDiv = document.createElement('div');
            messageDiv.className = `message ${message.username === username ? 'sent' : 'received'}`;
            
            // 添加时间戳
            const timeDiv = document.createElement('div');
            timeDiv.className = 'time';
            timeDiv.textContent = formatTimestamp(message.timestamp);
            
            // 添加用户名（接收到的消息才显示）
            if (message.username !== username) {
                const usernameSpan = document.createElement('span');
                usernameSpan.className = 'username';
                usernameSpan.textContent = message.username;
                messageDiv.appendChild(usernameSpan);
            }
            
            // 创建气泡
            const bubbleDiv = document.createElement('div');
            bubbleDiv.className = 'bubble';
            bubbleDiv.textContent = message.content;
            
            messageDiv.appendChild(timeDiv);
            messageDiv.appendChild(bubbleDiv);
            messagesDiv.appendChild(messageDiv);
        });
        
        // 滚动到底部
        messagesDiv.scrollTop = messagesDiv.scrollHeight;
    }
    
    // Poll messages
    function pollMessages() {
        if (!currentRoom) {
            return;
        }
        
        try {
            fetch(`/messages?room=${currentRoom}`)
                .then(response => {
                    if (!response.ok) {
                        throw new Error(`HTTP error! status: ${response.status}`);
                    }
                    return response.json();
                })
                .then(messages => {
                    // 检查是否有新消息
                    if (hasNewMessages(currentMessages, messages)) {
                        console.log('New messages detected, updating display');
                        displayMessages(messages);
                        currentMessages = messages;
                    } else {
                        console.log('No new messages');
                    }
                })
                .catch(error => {
                    console.error('Error polling messages:', error);
                    
                    if (retryDelay < 10000) { // 最大重试延迟为10秒
                        retryDelay *= 2;
                    }
                    
                    setTimeout(pollMessages, retryDelay);
                    return;
                });
        } catch (error) {
            console.error('Error polling messages:', error);
            
            if (retryDelay < 10000) { // 最大重试延迟为10秒
                retryDelay *= 2;
            }
            
            setTimeout(pollMessages, retryDelay);
            return;
        }
        
        // 成功获取消息后重置重试延迟
        retryDelay = 1000;
    }
    
    function hasNewMessages(oldMessages, newMessages) {
        // 如果消息数量不同，说明有新消息
        if (oldMessages.length !== newMessages.length) {
            return true;
        }
        
        // 比较最后一条消息
        if (oldMessages.length > 0 && newMessages.length > 0) {
            const lastOldMessage = oldMessages[oldMessages.length - 1];
            const lastNewMessage = newMessages[newMessages.length - 1];
            
            // 比较消息内容和时间戳
            return lastOldMessage.content !== lastNewMessage.content ||
                   lastOldMessage.timestamp !== lastNewMessage.timestamp ||
                   lastOldMessage.username !== lastNewMessage.username;
        }
        
        return false;
    }
    
    // 发送消息
    async function sendMessage() {
        const messageInput = document.getElementById('messageInput');
        const content = messageInput.value.trim();
        
        if (!content || !currentRoom || !username) {
            alert('Please join a room and enter a message');
            return;
        }
        
        try {
            const response = await fetch('/send_message', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    room: currentRoom,
                    username: username,
                    content: content
                })
            });
            
            const data = await response.json();
            if (response.ok) {
                messageInput.value = '';  // 清空输入框
            } else {
                alert(data.error || 'Failed to send message');
            }
        } catch (error) {
            console.error('Send message error:', error);
            alert('Failed to send message');
        }
    }
    
    // 创建房间
    async function createRoom() {
        const roomName = prompt('Enter room name:');
        if (!roomName) return;
        
        try {
            const response = await fetch('/create_room', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({ 
                    name: roomName,
                    username: username  // 添加当前用户名
                })
            });
            
            const data = await response.json();
            if (response.ok) {
                // 创建成功后立即加入房间
                await joinRoom(roomName);
                await updateRoomList();  // 更新房间列表
            } else {
                alert(data.error || 'Failed to create room');
            }
        } catch (error) {
            console.error('Create room error:', error);
            alert('Failed to create room');
        }
    }
    
    // 加入房间
    async function joinRoom(roomName) {
        if (!roomName || !username) return;
        
        try {
            const response = await fetch('/join_room', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({ 
                    room: roomName,
                    username: username 
                })
            });
            
            const data = await response.json();
            if (response.ok) {
                currentRoom = roomName;
                document.getElementById('currentRoom').textContent = `Current Room: ${roomName}`;
                // 清空消息列表
                const messageList = document.getElementById('messageList');
                messageList.innerHTML = '';
                // 更新房间列表以显示当前房间
                await updateRoomList();
                // 开始轮询新消息
                if (!isPolling) {
                    pollMessages();
                    isPolling = true;
                }
            } else {
                alert(data.error || 'Failed to join room');
            }
        } catch (error) {
            console.error('Join room error:', error);
            alert('Failed to join room');
        }
    }
    
    // 更新房间列表
    async function updateRoomList() {
        try {
            const response = await fetch('/rooms');
            const rooms = await response.json();
            
            const roomList = document.getElementById('roomList');
            roomList.innerHTML = '';
            
            rooms.forEach(room => {
                const roomElement = document.createElement('div');
                roomElement.className = 'room-item';
                roomElement.textContent = room.name;
                
                // 显示房间成员数量
                const memberCount = room.members.length;
                const memberInfo = document.createElement('span');
                memberInfo.className = 'member-count';
                memberInfo.textContent = `(${memberCount})`;
                roomElement.appendChild(memberInfo);
                
                if (currentRoom === room.name) {
                    roomElement.classList.add('active');
                }
                
                roomElement.onclick = () => joinRoom(room.name);
                roomList.appendChild(roomElement);
            });
            
            // 如果当前房间不在列表中，清除当前房间
            if (currentRoom && !rooms.some(room => room.name === currentRoom)) {
                currentRoom = null;
                document.getElementById('currentRoom').textContent = 'No Room Selected';
            }
        } catch (error) {
            console.error('Update room list error:', error);
        }
    }
    
    // 初始化聊天
    async function initialize() {
        try {
            // 先获取房间列表
            await updateRoomList();
            
            // 获取第一个房间或等待创建新房间
            const response = await fetch('/rooms');
            const rooms = await response.json();
            
            if (rooms.length > 0) {
                // 如果有房间，加入第一个房间
                await joinRoom(rooms[0].name);
            }
            
            // 开始定期更新房间列表
            setInterval(updateRoomList, 5000);
            
            // 添加创建房间按钮的事件监听器
            document.getElementById('createRoomButton').onclick = createRoom;
            
            // 添加发送消息的事件监听器
            const messageInput = document.getElementById('messageInput');
            const sendButton = document.getElementById('sendButton');
            
            sendButton.onclick = () => sendMessage();
            messageInput.onkeypress = (e) => {
                if (e.key === 'Enter' && !e.shiftKey) {
                    e.preventDefault();
                    sendMessage();
                }
            };
        } catch (error) {
            console.error('Initialize error:', error);
            throw error;
        }
    }
});
