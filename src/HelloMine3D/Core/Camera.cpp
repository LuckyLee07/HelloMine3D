#include "Camera.h"

#include "../Maths/Matrix.h"

Camera::Camera(const Config &config) noexcept
    : m_config(config)
{
    m_projectionMatrix = makeProjectionMatrix(config);

    position = {0, 0, -3.5};
}

void Camera::update() noexcept
{
    update(m_pEntity->position, m_pEntity->rotation);
}

void Camera::update(const glm::vec3 &targetPosition,
                    const glm::vec3 &targetRotation) noexcept
{
    position = {targetPosition.x, targetPosition.y + 0.6f,
                targetPosition.z};
    rotation = targetRotation;

    m_viewMatrix = makeViewMatrix(*this);
    m_projViewMatrx = m_projectionMatrix * m_viewMatrix;
    m_frustum.update(m_projViewMatrx);
}

void Camera::setFov(int fov) noexcept
{
    if (m_config.fov == fov) {
        return;
    }
    m_config.fov = fov;
    m_projectionMatrix = makeProjectionMatrix(m_config);
    m_projViewMatrx = m_projectionMatrix * m_viewMatrix;
    m_frustum.update(m_projViewMatrx);
}

void Camera::hookEntity(const Entity &entity) noexcept
{
    m_pEntity = &entity;
}

const glm::mat4 &Camera::getViewMatrix() const noexcept
{
    return m_viewMatrix;
}

const glm::mat4 &Camera::getProjMatrix() const noexcept
{
    return m_projectionMatrix;
}

const glm::mat4 &Camera::getProjectionViewMatrix() const noexcept
{
    return m_projViewMatrx;
}

const ViewFrustum &Camera::getFrustum() const noexcept
{
    return m_frustum;
}
