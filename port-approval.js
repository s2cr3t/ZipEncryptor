/**
 * 端口开通审批功能脚本
 */

// 模拟角色定义
const ROLE = {
    APPLICANT: 'applicant',  // 提交人
    APPROVER: 'approver',    // 审批人
    EXECUTOR: 'executor'     // 执行人
};

// 模拟当前用户信息
const currentUser = {
    id: 1,
    name: '张三',
    role: [ROLE.APPLICANT, ROLE.APPROVER]  // 模拟用户可以有多个角色
};

// 状态定义
const STATUS = {
    DRAFT: 'draft',           // 草稿
    PENDING: 'pending',       // 待审批
    APPROVED: 'approved',     // 已批准
    REJECTED: 'rejected',     // 已驳回
    RECALLED: 'recalled',     // 已撤回
    COMPLETED: 'completed'    // 已完成
};

// 申请类型
const WORKFLOW_TYPES = {
    INTERNAL: 'internal',     // 内部端口
    EXTERNAL: 'external',     // 外部端口
    DMZ: 'dmz',               // DMZ区端口
    VPN: 'vpn',               // VPN端口
    TEMPORARY: 'temporary',   // 临时端口
    OTHER: 'other'            // 其他类型
};

// 申请类型显示名称
const WORKFLOW_TYPE_NAMES = {
    [WORKFLOW_TYPES.INTERNAL]: '内部端口开通',
    [WORKFLOW_TYPES.EXTERNAL]: '外部端口开通',
    [WORKFLOW_TYPES.DMZ]: 'DMZ区端口开通',
    [WORKFLOW_TYPES.VPN]: 'VPN端口开通',
    [WORKFLOW_TYPES.TEMPORARY]: '临时端口开通',
    [WORKFLOW_TYPES.OTHER]: '其他类型'
};

// 模拟用户数据
const mockUsers = [
    { id: 1, name: '张三', role: [ROLE.APPLICANT, ROLE.APPROVER], department: '运维部' },
    { id: 2, name: '李四', role: [ROLE.APPROVER], department: '安全部' },
    { id: 3, name: '王五', role: [ROLE.EXECUTOR], department: '网络部' },
    { id: 4, name: '赵六', role: [ROLE.APPLICANT], department: '研发部' },
    { id: 5, name: '钱七', role: [ROLE.APPROVER, ROLE.EXECUTOR], department: '网络部' }
];

// 模拟工单数据
let mockWorkflows = [
    {
        id: 'PORT-20230501-001',
        type: WORKFLOW_TYPES.INTERNAL,
        typeDesc: '',
        applicantId: 4,
        applicant: '赵六',
        createTime: '2023-05-01 09:30:00',
        deadline: '2023-06-01',
        status: STATUS.PENDING,
        currentStep: 'approval',
        currentHandlers: [1, 2],
        portInfo: [
            {
                sourceIp: '192.168.1.10',
                sourcePort: '8080',
                destIp: '192.168.2.20',
                destPort: '3306',
                protocol: 'TCP',
                purpose: '数据库连接'
            }
        ],
        reason: '需要连接内部数据库进行数据同步',
        attachments: [
            { name: '申请说明.docx', url: '#', size: '15KB' }
        ],
        timeline: [
            {
                step: 'submission',
                status: STATUS.PENDING,
                handler: '赵六',
                handlerId: 4,
                time: '2023-05-01 09:30:00',
                comment: '提交申请'
            },
            {
                step: 'approval',
                status: STATUS.PENDING,
                handler: '',
                handlerId: null,
                time: '',
                comment: ''
            },
            {
                step: 'execution',
                status: STATUS.PENDING,
                handler: '',
                handlerId: null,
                time: '',
                comment: ''
            }
        ]
    }
];