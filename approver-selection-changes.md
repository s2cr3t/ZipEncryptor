# 审批人选择修改说明

## 主要修改点

### 1. HTML 输入类型改变
**之前（复选框）：**
```html
<input type="checkbox" id="approver-${user.id}" value="${user.id}">
```

**之后（单选按钮）：**
```html
<input type="radio" name="approver" id="approver-${user.id}" value="${user.id}">
```

### 2. 获取选中值的方式改变
**之前（获取多个选中的复选框）：**
```javascript
const selectedApprovers = Array.from(document.querySelectorAll('input[type="checkbox"]:checked'))
    .map(cb => parseInt(cb.value));
```

**之后（获取单个选中的单选按钮）：**
```javascript
const selectedApprover = document.querySelector('input[name="approver"]:checked');
const approverId = selectedApprover ? parseInt(selectedApprover.value) : null;
```

### 3. 数据结构保持不变
虽然现在只选择一个审批人，但 `currentHandlers` 仍然保持数组格式：
```javascript
workflow.currentHandlers = [approverId];  // 仍然是数组，但只包含一个元素
```

### 4. 验证逻辑改变
**之前：**
```javascript
if (selectedApprovers.length === 0) {
    alert('请至少选择一个审批人');
}
```

**之后：**
```javascript
if (!selectedApprover) {
    alert('请选择一个审批人');
}
```

## 关键点
- 所有单选按钮必须有相同的 `name` 属性（这里使用 "approver"）
- 单选按钮会自动实现互斥，用户只能选择一个
- 其他代码逻辑保持不变，只是选择方式从多选变为单选