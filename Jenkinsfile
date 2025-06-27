pipeline {
    agent any

    stages {
        stage('Clone') {
            steps {
                git 'https://github.com/minh-9999/multithread-progress-tracker.git'
            }
        }

        stage('Build') {
            steps {
                sh '''
                    mkdir -p build
                    cd build
                    cmake ..
                    make -j$(nproc)
                '''
            }
        }

        stage('Test') {
            steps {
                sh './build./Multithread-Progress-Tracker'
            }
        }
    }
}
