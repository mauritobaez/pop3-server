const Pop3Command = require('node-pop3');
const NetcatClient = require('netcat/client');

const clients = 900;
let finished = 0;
let totaltime = 0;

function addUsers(users) {
    const nc = new NetcatClient();
    return new Promise((resolve) => {
        const client = nc.addr('::1').port(2110).connect().send('a root root\r\n').on('data', (buffer) => {
        });
        const total = { count: 0 };
        for (let i = 0; i < users; i += 1) {
            client.send(`u+ ${i} ${i}\r\n`, () => {
                total.count += 1;
                console.log(total.count);
                if (total.count === users) {
                    client.send('q\r\n', () => {
                        client.close();
                        resolve();
                    });
                }
            })
        }
        client.send('q\r\n');
    });
}

function parseNanoToMSeconds(hrtime) {
    var seconds = (hrtime[0] + (hrtime[1] / 1e6));
    return seconds;
}

async function getEmail(index) {
    const pop3 = new Pop3Command({
        user: index,
        password: index,
        host: "localhost",
        port: 1110
    });
    const start = process.hrtime();

    await pop3.connect();
    finished += 1;
    await pop3.command('USER', index);
    await pop3.command('PASS', index);
    await pop3.command('RETR', 1);
    await pop3.command('QUIT');
    const seconds = parseNanoToMSeconds(process.hrtime(start));
    totaltime += seconds;
    console.log(totaltime / finished);
    return seconds;
}

async function start() {
    let connections;
    try {
        connections = parseInt(process.argv.at(2), 10);
    } catch (e) {
        connections = clients;
    }
    await addUsers(connections);
    const emails = [];
    for (let i = 0; i < connections; i += 1) {
        setTimeout(() => {
            emails.push(getEmail(i));
        }, i * 3);
    }
    const seconds = await Promise.all(emails);
}

start();